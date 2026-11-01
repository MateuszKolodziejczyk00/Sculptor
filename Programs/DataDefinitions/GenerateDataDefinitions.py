"""
Generates C++ headers from Sculptor Data Definition (.sdd) files.

Usage:
    GenerateDataDefinitions.py <OutDir> <File.sdd>...   generates given files
    GenerateDataDefinitions.py <OutDir>                 generates all .sdd files from OutDir's parent and removes stale headers
"""

import hashlib
import re
import sys
from pathlib import Path


TOKEN_RE = re.compile(r'''
	\s+|//[^\n]*|/\*.*?\*/
	|(?P<string>"(?:\\.|[^"\\\n])*")
	|(?P<number>0[xX][0-9a-fA-F]+[uU]?|(?:\d+\.\d*|\.\d+|\d+)(?:[eE][+-]?\d+)?[fFuU]?)
	|(?P<ident>[A-Za-z_]\w*(?:::[A-Za-z_]\w*)*)
	|(?P<punct>[{}\[\]()<>;:,=.+\-*/|&~])
	|(?P<error>.)
	''', re.VERBOSE | re.DOTALL)

BRACKET_DEPTH = { '(': 1, '[': 1, '{': 1, ')': -1, ']': -1, '}': -1 }

GENERATOR_HASH = hashlib.sha1(Path(__file__).read_bytes()).hexdigest()


class SddError(Exception):
	pass


class Member:
	def __init__(self, type, name, default, testDefault):
		self.type = type
		self.name = name
		self.default = default
		self.testDefault = testDefault


class Struct:
	def __init__(self, name, generator, members):
		self.name = name
		self.generator = generator
		self.members = members


class Enum:
	def __init__(self, name, underlyingType, values):
		self.name = name
		self.underlyingType = underlyingType
		self.values = values


class Namespace:
	def __init__(self, name, items):
		self.name = name
		self.items = items


class Parser:
	def __init__(self, path, text):
		self.path = path
		self.text = text
		self.tokens = [ (m.lastgroup, m.group(), m.start()) for m in TOKEN_RE.finditer(text) if m.lastgroup ]
		self.tokens.append(('eof', '<end of file>', len(text)))
		self.pos = 0
		self.structNames = set()
		for token in self.tokens:
			if token[0] == 'error':
				self.Fail(token, f"unexpected character '{token[1]}'")

	def Fail(self, token, message):
		offset = token[2]
		line = self.text.count('\n', 0, offset) + 1
		col = offset - self.text.rfind('\n', 0, offset)
		raise SddError(f"{self.path}({line},{col}): error SDD: {message}")

	def Peek(self):
		return self.tokens[self.pos]

	def Next(self):
		token = self.tokens[self.pos]
		self.pos += 1
		return token

	def Accept(self, text):
		kind, tokenText, _ = self.tokens[self.pos]
		if tokenText == text and kind != 'string':
			self.pos += 1
			return True
		return False

	def Expect(self, text):
		if not self.Accept(text):
			self.Fail(self.Peek(), f"expected '{text}', got '{self.Peek()[1]}'")

	def ExpectKind(self, kind):
		token = self.Next()
		if token[0] != kind:
			self.Fail(token, f"expected {kind}, got '{token[1]}'")
		return token

	def ParseFile(self):
		includes = []
		while self.Accept('include'):
			includeToken = self.ExpectKind('string')
			include = includeToken[1]
			if include.endswith('.sdd"'):
				includePath = self.path.parent / include[1:-1]
				if not includePath.exists():
					self.Fail(includeToken, f"included file {includePath} does not exist")
				self.structNames |= Parser(includePath, includePath.read_text(encoding='utf-8')).ParseFile()[2]
				includes.append(f'"Generated/{includePath.stem}.h"')
			else:
				includes.append(include)
			self.Expect(';')

		items = self.ParseItems()
		self.ExpectKind('eof')
		return includes, items, self.structNames

	def ParseItems(self):
		items = []
		while self.Peek()[0] != 'eof' and self.Peek()[1] != '}':
			if self.Accept('namespace'):
				name = self.ExpectKind('ident')[1]
				self.Expect('{')
				items.append(Namespace(name, self.ParseItems()))
				self.Expect('}')
			elif self.Peek()[1] == 'include':
				self.Fail(self.Peek(), "includes must be at the top of the file")
			else:
				annotations = self.ParseAnnotations()
				if self.Accept('enum'):
					items.append(self.ParseEnum(annotations))
				else:
					struct = self.ParseStruct(annotations)
					self.structNames.add(struct.name)
					items.append(struct)
		return items

	def ParseAnnotations(self):
		annotations = []
		while self.Accept('['):
			while True:
				token = self.ExpectKind('ident')
				value = None
				if self.Accept('='):
					value = self.ParseExpression((',', ']'))
				elif self.Accept('('):
					self.ParseAnnotationValue()
					while self.Accept(','):
						self.ParseAnnotationValue()
					self.Expect(')')
				annotations.append((token, value))
				if not self.Accept(','):
					break
			self.Expect(']')
		return annotations

	def ParseAnnotationValue(self):
		self.Accept('-')
		token = self.Next()
		if token[0] not in ('number', 'string', 'ident'):
			self.Fail(token, f"invalid annotation value '{token[1]}'")

	def ParseExpression(self, terminators):
		tokens = []
		depth = 0
		while depth or self.Peek()[1] not in terminators:
			token = self.Next()
			depth += BRACKET_DEPTH.get(token[1], 0)
			if token[0] == 'eof' or depth < 0:
				self.Fail(token, 'expected ' + ' or '.join(f"'{t}'" for t in terminators))
			tokens.append(token)
		if not tokens:
			self.Fail(self.Peek(), "expected value")
		return JoinTokens(tokens, appendFloatSuffix=True)

	def ParseEnum(self, annotations):
		for token, _ in annotations:
			self.Fail(token, f"unknown enum annotation '{token[1]}'")

		name = self.ExpectKind('ident')[1]
		underlyingType = self.ExpectKind('ident')[1] if self.Accept(':') else 'Uint32'
		self.Expect('{')

		values = []
		while not self.Accept('}'):
			self.ParseAnnotations()
			valueName = self.ExpectKind('ident')[1]
			values.append((valueName, self.ParseExpression((',', '}')) if self.Accept('=') else None))
			if not self.Accept(','):
				self.Expect('}')
				break
		self.Expect(';')

		return Enum(name, underlyingType, values)

	def ParseStruct(self, annotations):
		generator = None
		for token, value in annotations:
			if token[1] != 'Gen':
				self.Fail(token, f"unknown struct annotation '{token[1]}'")
			if generator is not None:
				self.Fail(token, "duplicate 'Gen' annotation")
			if value not in STRUCT_GENERATORS:
				self.Fail(token, f"'Gen' annotation requires one of: {', '.join(STRUCT_GENERATORS)}")
			generator = STRUCT_GENERATORS[value]

		self.Expect('struct')
		name = self.ExpectKind('ident')[1]
		self.Expect('{')

		members = []
		while not self.Accept('}'):
			members.append(self.ParseMember(self.ParseAnnotations()))
		self.Expect(';')

		return Struct(name, generator or StructGen, members)

	def ParseMember(self, annotations):
		testDefault = None
		for token, value in annotations:
			if token[1] == 'test':
				if value is None:
					self.Fail(token, "'test' annotation requires a value")
				if testDefault is not None:
					self.Fail(token, "duplicate 'test' annotation")
				testDefault = value

		typeTokens = [ self.ExpectKind('ident') ]
		if self.Peek()[1] == '<':
			depth = 0
			while True:
				token = self.Next()
				if token[0] == 'eof':
					self.Fail(token, "unterminated template argument list")
				typeTokens.append(token)
				depth += { '<': 1, '>': -1 }.get(token[1], 0)
				if depth == 0:
					break

		name = self.ExpectKind('ident')[1]

		default = self.ParseExpression((';',)) if self.Accept('=') else None

		self.Expect(';')
		return Member(JoinTokens(typeTokens, appendFloatSuffix=False), name, default, testDefault)


def IsUnsuffixedFloat(text):
	return not text.startswith(('0x', '0X')) and any(c in text for c in '.eE') and text[-1] not in 'fF'


def JoinTokens(tokens, appendFloatSuffix):
	result = []
	for kind, text, _ in tokens:
		if appendFloatSuffix and kind == 'number' and IsUnsuffixedFloat(text):
			text += 'f'
		result.append(text + ' ' if text == ',' else text)
	return ''.join(result)


def MacroArg(text):
	return f'SPT_SINGLE_ARG({text})' if ',' in text else text


class StructGen:
	includes = []

	@staticmethod
	def Generate(struct, lines):
		lines += [ f'struct {struct.name}', '{' ]
		lines += [ f'\t{m.type} {m.name} = {m.default if m.default is not None else "{}"};' for m in struct.members ]
		lines += [ '', '\tvoid Serialize(srl::Serializer& serializer)', '\t{' ]
		lines += [ f'\t\tserializer.Serialize("{m.name}", {m.name});' for m in struct.members ]
		lines += [ '\t}', '};' ]


class ShaderStructGen:
	includes = [ '"ShaderStructs/ShaderStructs.h"' ]

	@staticmethod
	def Generate(struct, lines):
		lines.append(f'BEGIN_SHADER_STRUCT({struct.name})')
		for m in struct.members:
			if m.default is None:
				lines.append(f'\tSHADER_STRUCT_FIELD({MacroArg(m.type)}, {m.name})')
			else:
				lines.append(f'\tSHADER_STRUCT_FIELD_DEFAULT({MacroArg(m.type)}, {m.name}, {MacroArg(m.default)})')
		lines += [ 'END_SHADER_STRUCT();', '', '', f'inline void SerializeType(srl::Serializer& serializer, {struct.name}& data)', '{' ]
		lines += [ f'\tserializer.Serialize("{m.name}", data.{m.name});' for m in struct.members ]
		lines.append('}')


STRUCT_GENERATORS = { generator.__name__: generator for generator in (StructGen, ShaderStructGen) }


def GenerateHeader(sourceName, sourceHash, includes, items, structNames):
	body = []
	generators = []
	GenerateItems(items, structNames, generators, body)

	lines = [
		f'// Generated from {sourceName}. Do not edit.',
		f'// SDDHash: {sourceHash}',
		'#pragma once',
		'',
		'#include "SculptorCoreTypes.h"',
		'#include "Serialization.h"',
	]
	lines += [ f'#include {include}' for generator in generators for include in generator.includes ]
	lines += [ f'#include {include}' for include in includes ]
	lines += [ '', '' ] + body
	return '\n'.join(lines).rstrip('\n') + '\n'


def GenerateItems(items, structNames, generators, lines):
	for item in items:
		if isinstance(item, Namespace):
			lines += [ f'namespace {item.name}', '{', '' ]
			GenerateItems(item.items, structNames, generators, lines)
			lines[-1] = f'}} // {item.name}'
			lines += [ '', '' ]
			continue

		if isinstance(item, Enum):
			lines += [ f'enum class {item.name} : {item.underlyingType}', '{' ]
			lines += [ f'\t{name},' if value is None else f'\t{name} = {value},' for name, value in item.values ]
			lines += [ '};', '', '' ]
			continue

		struct = item
		if struct.generator not in generators:
			generators.append(struct.generator)
		struct.generator.Generate(struct, lines)

		testDefaults = []
		for m in struct.members:
			if m.testDefault is not None:
				testDefaults.append(f'\tdata.{m.name} = {m.testDefault};')
			elif m.type.split('::')[-1] in structNames:
				testDefaults.append(f'\tApplyTestDefaults(data.{m.name});')
		lines += [ '', '', f'inline void ApplyTestDefaults({struct.name}&{" data" if testDefaults else ""})', '{' ] + testDefaults + [ '}', '', '' ]


def Generate(sddPath, outDir):
	sourceName = sddPath.relative_to(outDir.parent).as_posix()
	outPath = outDir / (sddPath.stem + '.h')
	sddBytes = sddPath.read_bytes()
	sourceHash = hashlib.sha1(sddBytes + GENERATOR_HASH.encode()).hexdigest()

	if outPath.exists():
		with outPath.open(encoding='utf-8') as outFile:
			header = [ outFile.readline().rstrip('\n'), outFile.readline().rstrip('\n') ]
		if header[0] != f'// Generated from {sourceName}. Do not edit.':
			raise SddError(f"{sddPath}(1,1): error SDD: {outPath.name} is already generated from a different file ({header[0]})")
		if header[1] == f'// SDDHash: {sourceHash}':
			return

	includes, items, structNames = Parser(sddPath, sddBytes.decode('utf-8')).ParseFile()
	outDir.mkdir(parents=True, exist_ok=True)
	outPath.write_text(GenerateHeader(sourceName, sourceHash, includes, items, structNames), encoding='utf-8', newline='\n')


def Main(args):
	outDir = Path(args[0]).resolve()
	if len(args) > 1:
		sddPaths = [ Path(arg).resolve() for arg in args[1:] ]
	else:
		sourceDir = outDir.parent
		sddPaths = [ p for p in sourceDir.rglob('*.sdd') if p.relative_to(sourceDir).parts[0] not in ('ThirdParty', outDir.name) ]
		names = {}
		for p in sddPaths:
			if p.stem in names:
				raise SddError(f"{p}(1,1): error SDD: file name clashes with {names[p.stem]}")
			names[p.stem] = p
		if outDir.exists():
			for header in outDir.glob('*.h'):
				if header.stem not in names:
					header.unlink()

	for sddPath in sddPaths:
		Generate(sddPath, outDir)


if __name__ == '__main__':
	try:
		Main(sys.argv[1:])
	except SddError as error:
		print(error, file=sys.stderr)
		sys.exit(1)
