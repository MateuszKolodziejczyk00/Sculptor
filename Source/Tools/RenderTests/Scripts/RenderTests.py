"""Render tests orchestrator: golden images (NVIDIA FLIP) + GPU performance regression tracking.

Usage (or RunRenderTests.bat in engine directory with the same arguments, "run" if none given):
  python RenderTests.py run [--mode fast|robust] [--filter <glob>] [--config Development] [--label <text>] [--no-open]
  python RenderTests.py accept [<test> ...] [--failed] [--passed] [--run <run id>]
  python RenderTests.py report [--no-open]

Tests:   Tests/GoldenImages/Scenes/<Test>.json (+ golden <Test>.png)
Results: Saved/RenderTests/Runs/<run id>/, dashboard: Saved/RenderTests/Report.html
Requires: pip install flip-evaluator pillow numpy
"""

import argparse
import datetime
import fnmatch
import json
import os
import pathlib
import platform
import shutil
import statistics
import subprocess
import sys
import time
import webbrowser

import numpy as np
import flip_evaluator as flip
from PIL import Image


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ENGINE_DIR = os.path.normpath(os.path.join(SCRIPT_DIR, "..", "..", "..", ".."))
SCENE_TESTS_DIR = os.path.join(ENGINE_DIR, "Tests", "GoldenImages", "Scenes")
OUTPUT_DIR = os.path.join(ENGINE_DIR, "Saved", "RenderTests")
RUNS_DIR = os.path.join(OUTPUT_DIR, "Runs")
LOCAL_OUTPUT_DIR = os.path.join(OUTPUT_DIR, "Local")
REPORT_TEMPLATE_PATH = os.path.join(SCRIPT_DIR, "ReportTemplate.html")
REPORT_PATH = os.path.join(OUTPUT_DIR, "Report.html")

DEFAULT_MAX_FLIP_MEAN = 0.0
PERF_FILE = "Perf.json"
HASHES_FILE = "Hashes.json"
SETTINGS_FILE = "Settings.json"
SAMPLES_FILE = "Samples.js"
TRANSIENT_MEMORY_FILE = "TransientMemory.js"
GOLDEN_HASHES_SUFFIX = ".hashes.json"
GOLDEN_PIPELINE_STATISTICS_SUFFIX = ".pipelinestats.json"
GOLDEN_SETTINGS_SUFFIX = ".settings.json"
GOLDEN_WORKLOAD_SUFFIX = ".workload.json"
WORKLOAD_KEYS = ("FSInvocations", "CSInvocations", "TSInvocations")
# FS invocations depend on hardware scheduling, so they vary slightly between runs
FS_INVOCATIONS_CHANGE_THRESHOLD = 0.1


def LoadJson(path):
    with open(path, "r") as file:
        return json.load(file)


def SaveJson(path, data):
    with open(path, "w") as file:
        json.dump(data, file, indent=1)


def Git(*args):
    try:
        return subprocess.run(["git", *args], cwd=ENGINE_DIR, capture_output=True, text=True, check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return ""


def DiscoverTests(pattern):
    tests = {}
    for fileName in sorted(os.listdir(SCENE_TESTS_DIR)):
        name, ext = os.path.splitext(fileName)
        if ext == ".json" and not fileName.endswith((GOLDEN_HASHES_SUFFIX, GOLDEN_PIPELINE_STATISTICS_SUFFIX, GOLDEN_SETTINGS_SUFFIX, GOLDEN_WORKLOAD_SUFFIX)) and fnmatch.fnmatch(name, pattern):
            tests[name] = os.path.join(SCENE_TESTS_DIR, fileName)
    return tests


def LoadRuns():
    runs = []
    if os.path.isdir(RUNS_DIR):
        for runId in sorted(os.listdir(RUNS_DIR)):
            runInfoPath = os.path.join(RUNS_DIR, runId, "Run.json")
            if os.path.exists(runInfoPath):
                runs.append(LoadJson(runInfoPath))
    return runs


# Execution =========================================================================================

def RunnerCommand(args):
    command = [os.path.join(ENGINE_DIR, "Binaries", "{}_windows_x86_64".format(args.config), "RenderTests.exe")]
    if not args.no_ray_tracing:
        command.append("-EnableRayTracing")
    if args.validation:
        command.append("-EnableValidation")
    return command


def LaunchRunner(args, runDir, testNames, testPaths, logPath, timeout):
    command = RunnerCommand(args)
    if not os.path.exists(command[0]):
        sys.exit("RenderTests executable not found: {}".format(command[0]))

    testsListPath = os.path.join(runDir, "TestsList.txt")
    with open(testsListPath, "w") as file:
        file.write("\n".join(testPaths[name] for name in testNames))

    command += ["-TestsList=" + testsListPath, "-Output=" + runDir]

    with open(logPath, "w") as logFile:
        try:
            process = subprocess.run(command, cwd=os.path.dirname(command[0]), stdout=logFile, stderr=subprocess.STDOUT, timeout=timeout)
            return "Crashed" if process.returncode != 0 else None
        except subprocess.TimeoutExpired:
            return "Timeout"


def IsFinished(runDir, testName):
    return os.path.exists(os.path.join(runDir, testName, PERF_FILE))


def ExecuteTests(args, runDir, testPaths):
    """Returns {test name: failure status} for tests that didn't finish."""
    failures = {}

    if args.mode == "robust":
        for name in testPaths:
            print("[{}] running...".format(name))
            os.makedirs(os.path.join(runDir, name), exist_ok=True)
            failure = LaunchRunner(args, runDir, [name], testPaths, os.path.join(runDir, name, "Log.txt"), args.timeout)
            if not IsFinished(runDir, name):
                failures[name] = failure or "Crashed"
        return failures

    # Fast mode: all tests in one process. If the process dies, the test that was in progress is marked as failed and the rest is resumed in a new process
    remaining = list(testPaths)
    launchIdx = 0
    while remaining:
        print("Running {} test(s) in one process...".format(len(remaining)))
        failure = LaunchRunner(args, runDir, remaining, testPaths, os.path.join(runDir, "Log_{}.txt".format(launchIdx)), args.timeout * len(remaining))
        launchIdx += 1

        remaining = [name for name in remaining if not IsFinished(runDir, name)]
        if remaining:
            failures[remaining[0]] = failure or "Crashed"
            remaining = remaining[1:]

    return failures


# Evaluation ========================================================================================

def LoadRGB(path):
    return np.asarray(Image.open(path).convert("RGB"), dtype=np.float32) / 255.0


def EvaluateImage(testDir, goldenPath, maxFlipMean):
    if not os.path.exists(goldenPath):
        return "NoGolden", None

    shutil.copyfile(goldenPath, os.path.join(testDir, "Golden.png"))

    golden = LoadRGB(goldenPath)
    result = LoadRGB(os.path.join(testDir, "Result.png"))
    if golden.shape != result.shape:
        return "Fail", None

    # Changed pixels in red over dimmed result
    diff = np.repeat(result.mean(axis=2, keepdims=True) * 0.3, 3, axis=2)
    diff[np.any(golden != result, axis=2)] = (1.0, 0.0, 0.0)
    Image.fromarray((diff * 255.0 + 0.5).astype(np.uint8)).save(os.path.join(testDir, "Diff.png"))

    magmaMap, _, _ = flip.evaluate(golden, result, "LDR")
    Image.fromarray((np.clip(magmaMap, 0.0, 1.0) * 255.0 + 0.5).astype(np.uint8)).save(os.path.join(testDir, "Flip.png"))

    errorMap, mean, _ = flip.evaluate(golden, result, "LDR", applyMagma=False)
    errors = np.asarray(errorMap, dtype=np.float64).ravel()
    median, p95, p99 = np.percentile(errors, [50, 95, 99])

    flipStats = { "Mean": float(mean), "Median": float(median), "P95": float(p95), "P99": float(p99), "Max": float(errors.max()) }
    return ("Pass" if mean <= maxFlipMean else "Fail"), flipStats


def PassesWithOccurrences(passes):
    """Pass names repeat (e.g. per cascade), so passes are identified by (name, occurrence index)."""
    occurrences = {}
    for renderPass in passes:
        occurrence = occurrences.get(renderPass["Name"], 0)
        occurrences[renderPass["Name"]] = occurrence + 1
        yield (renderPass["Name"], occurrence), { output["Name"]: output["Hash"] for output in renderPass["Outputs"] }


def EvaluatePassHashes(testDir, goldenHashesPath):
    """Compares render graph passes outputs hashes of the golden frame with hashes stored with golden image."""
    if not os.path.exists(goldenHashesPath):
        return None

    goldenPasses = dict(PassesWithOccurrences(LoadJson(goldenHashesPath)["Passes"]))

    passes = []
    for (name, occurrence), outputs in PassesWithOccurrences(LoadJson(os.path.join(testDir, HASHES_FILE))["Passes"]):
        goldenOutputs = goldenPasses.get((name, occurrence))
        if not outputs and not goldenOutputs:
            continue

        if goldenOutputs is None:
            passes.append({ "Name": name, "Occurrence": occurrence, "Status": "New", "MismatchedOutputs": [] })
            continue

        mismatched = sorted(output for output in set(outputs) | set(goldenOutputs) if outputs.get(output) != goldenOutputs.get(output))
        passes.append({ "Name": name, "Occurrence": occurrence, "Status": "Mismatch" if mismatched else "Match", "MismatchedOutputs": mismatched })

    firstMismatch = next((p for p in passes if p["Status"] == "Mismatch"), None)
    return {
        "Matched": sum(p["Status"] == "Match" for p in passes),
        "Mismatched": sum(p["Status"] == "Mismatch" for p in passes),
        "New": sum(p["Status"] == "New" for p in passes),
        "FirstMismatch": firstMismatch["Name"] if firstMismatch else None,
        "Passes": passes,
    }


def MapPassHashesToScopes(passHashes, perf):
    """Assigns hash status to GPU profiler scopes (scope leaf name == render graph pass name). Parent scopes are mismatched if any child is."""
    statuses = { (p["Name"], p["Occurrence"]): p for p in passHashes["Passes"] }

    scopeStatus = {}
    scopeMismatches = {}
    occurrences = {}
    for path in perf["Scopes"]:
        leafName = path.rsplit("/", 1)[-1]
        occurrence = occurrences.get(leafName, 0)
        occurrences[leafName] = occurrence + 1

        passStatus = statuses.get((leafName, occurrence))
        if passStatus is None:
            continue

        scopeStatus[path] = passStatus["Status"]
        if passStatus["MismatchedOutputs"]:
            scopeMismatches[path] = passStatus["MismatchedOutputs"]

    statusPriority = { "Match": 0, "New": 1, "Mismatch": 2 }
    for path, status in list(scopeStatus.items()):
        parts = path.split("/")
        for depth in range(1, len(parts)):
            parentPath = "/".join(parts[:depth])
            parentStatus = scopeStatus.get(parentPath)
            if parentStatus is None or statusPriority[status] > statusPriority[parentStatus]:
                scopeStatus[parentPath] = status

    return scopeStatus, scopeMismatches


def EvaluatePipelineStatistics(perf, goldenPipelineStatisticsPath):
    """Returns {scope path: {statistic: golden value}} for statistics that differ from golden."""
    if not os.path.exists(goldenPipelineStatisticsPath):
        return None

    golden = LoadJson(goldenPipelineStatisticsPath)
    changes = {}
    for path, scope in perf["Scopes"].items():
        goldenStatistics = golden.get(path, {})
        changed = { name: goldenStatistics[name] for name, value in scope.get("PipelineStatistics", {}).items() if name in goldenStatistics and goldenStatistics[name] != value }
        if changed:
            changes[path] = changed
    return changes


def FlattenSettings(settings, prefix=""):
    flat = {}
    for key, value in settings.items():
        if isinstance(value, dict):
            flat.update(FlattenSettings(value, prefix + key + "."))
        else:
            flat[prefix + key] = value
    return flat


def EvaluateSettings(settings, goldenSettingsPath):
    """Returns {setting path: golden value} for settings that differ from golden (None if setting is not in golden)."""
    if not os.path.exists(goldenSettingsPath):
        return None

    current = FlattenSettings(settings)
    golden = FlattenSettings(LoadJson(goldenSettingsPath))
    return { path: golden.get(path) for path in current.keys() | golden.keys() if current.get(path) != golden.get(path) }


def TimingStats(samples):
    """P90 instead of max, so spikes caused by OS scheduling or clock changes don't count. Regressions are tracked on median, as it's the most stable."""
    minMs, medianMs, p90Ms = np.percentile(samples, [0, 50, 90])
    return { "MinMs": float(minMs), "Ms": float(medianMs), "P90Ms": float(p90Ms) }


def AggregatePerf(perfData):
    frames = perfData["Frames"]
    scopes = {}
    for frame in frames:
        for scope in frame["Scopes"]:
            entry = scopes.setdefault(scope["Path"], { "Samples": [] })
            entry["Samples"].append(scope["DurationMs"])
            for key in (*WORKLOAD_KEYS, "PipelineStatistics"):
                if key in scope:
                    entry[key] = scope[key]
            # Repeated paths (e.g. per cascade passes) keep the highest usage
            if "TransientMemoryUsage" in scope:
                entry["TransientMemoryUsage"] = max(entry.get("TransientMemoryUsage", 0), scope["TransientMemoryUsage"])

    for entry in scopes.values():
        entry.update(TimingStats(entry.pop("Samples")))

    memory = {
        "TotalAllocatedBytes": max(frame["MemoryStats"]["TotalAllocatedBytes"] for frame in frames),
        "PeakTransientBytes": max(scope["TransientMemoryUsage"] for scope in scopes.values() if "TransientMemoryUsage" in scope),
    }

    return { "GPUFrame": TimingStats([frame["GPUFrameMs"] for frame in frames]), "FramesNum": len(frames), "Scopes": scopes, "Memory": memory }


def IsComparableRun(run, runInfo):
    return all(run.get(key) == runInfo[key] for key in ("Machine", "Config", "Mode", "RayTracing"))


def MedianStats(statsList):
    return { key: statistics.median(stats[key] for stats in statsList) for key in ("MinMs", "Ms", "P90Ms") }


def ComputeBaseline(previousRuns, runInfo, testName, baselineRunsNum):
    # Runs from before min/P90 tracking store only median ("GPUFrameMs")
    candidates = [run for run in previousRuns
                  if IsComparableRun(run, runInfo) and "GPUFrame" in (run["Tests"].get(testName, {}).get("Perf") or {})]
    candidates = candidates[-baselineRunsNum:]
    if not candidates:
        return None

    perfs = [run["Tests"][testName]["Perf"] for run in candidates]
    scopePaths = set().union(*(perf["Scopes"].keys() for perf in perfs))
    scopes = { path: MedianStats([perf["Scopes"][path] for perf in perfs if path in perf["Scopes"]]) for path in scopePaths }

    # Memory usage is deterministic, so it's compared with the last run instead of median
    for path, scope in perfs[-1]["Scopes"].items():
        if "TransientMemoryUsage" in scope:
            scopes[path]["TransientMemoryUsage"] = scope["TransientMemoryUsage"]

    return {
        "Runs": [run["Id"] for run in candidates],
        "GPUFrame": MedianStats([perf["GPUFrame"] for perf in perfs]),
        "Scopes": scopes,
        "Memory": perfs[-1]["Memory"],
    }


def IsRegression(current, baseline, args):
    return current["Ms"] > baseline["Ms"] * (1.0 + args.perf_threshold) and current["Ms"] - baseline["Ms"] > args.perf_min_delta_ms


def EvaluatePerf(perf, baseline, args):
    if baseline is None:
        return "NoBaseline", []

    regressions = []
    if IsRegression(perf["GPUFrame"], baseline["GPUFrame"], args):
        regressions.append("GPU Frame")

    for path, scope in perf["Scopes"].items():
        if path in baseline["Scopes"] and path != "GPU Frame" and IsRegression(scope, baseline["Scopes"][path], args):
            regressions.append(path)

    return ("Regression" if regressions else "Ok"), regressions


def IsInvocationsChanged(key, current, golden):
    if key != "FSInvocations" or current is None or golden is None:
        return current != golden
    return abs(current - golden) > golden * FS_INVOCATIONS_CHANGE_THRESHOLD


def EvaluateWorkload(perf, goldenWorkloadPath):
    """Returns {scope path: {invocations key: golden value}} for invocation counts that differ from golden. Scopes that are not in golden are skipped."""
    if not os.path.exists(goldenWorkloadPath):
        return None

    golden = LoadJson(goldenWorkloadPath)
    changes = {}
    for path, scope in perf["Scopes"].items():
        if path in golden:
            changed = { key: golden[path].get(key) for key in WORKLOAD_KEYS if IsInvocationsChanged(key, scope.get(key), golden[path].get(key)) }
            if changed:
                changes[path] = changed
    return changes


# Ids are unique only within a frame, so they are not part of events compared between runs
def TransientEvents(scope, fields=("Name", "Size")):
    return [[[event[field] for field in fields] for event in scope.get(key, [])] for key in ("TransientAllocations", "TransientDeallocations")]


def TransientEventsPerPath(perfPath):
    """{scope path: [allocations, deallocations]} of the last measured frame. Repeated paths (e.g. per cascade passes) are concatenated in execution order."""
    events = {}
    for scope in LoadJson(perfPath)["Frames"][-1]["Scopes"]:
        if "TransientMemoryUsage" in scope:
            allocations, deallocations = events.setdefault(scope["Path"], [[], []])
            scopeAllocations, scopeDeallocations = TransientEvents(scope)
            allocations += scopeAllocations
            deallocations += scopeDeallocations
    return events


def EvaluateMemory(perf, baseline, testDir, testName):
    """Returns previous values of memory statistics that differ from the last comparable run.
    "Scopes": transient usage of passes, "Allocations": transient allocations and deallocations of passes."""
    if baseline is None:
        return None

    changes = { key: value for key, value in baseline["Memory"].items() if perf["Memory"][key] != value }
    changes["Scopes"] = {}

    for path, scope in perf["Scopes"].items():
        lastUsage = baseline["Scopes"].get(path, {}).get("TransientMemoryUsage")
        if scope.get("TransientMemoryUsage") != lastUsage:
            changes["Scopes"][path] = lastUsage

    events = TransientEventsPerPath(os.path.join(testDir, PERF_FILE))
    lastEvents = TransientEventsPerPath(os.path.join(RUNS_DIR, baseline["Runs"][-1], testName, PERF_FILE))
    paths = [*events, *(lastEvents.keys() - events.keys())]
    changes["Allocations"] = { path: lastEvents.get(path) for path in paths if events.get(path) != lastEvents.get(path) }

    return changes


def MemoryChangesNum(changes):
    return len(changes["Scopes"]) + len(changes["Allocations"]) + len(changes) - 2


def EvaluateRun(args, runDir, runInfo, testPaths, failures, previousRuns):
    for name, testPath in testPaths.items():
        testDir = os.path.join(runDir, name)
        maxFlipMean = LoadJson(testPath).get("MaxFlipMean", DEFAULT_MAX_FLIP_MEAN)

        # Paths relative to engine directory, command is meant to be run from there
        command = RunnerCommand(args) + ["-Test=" + os.path.relpath(testPath, ENGINE_DIR), "-Output=" + os.path.relpath(LOCAL_OUTPUT_DIR, ENGINE_DIR)]
        command[0] = os.path.relpath(command[0], ENGINE_DIR)

        result = { "ImageStatus": failures.get(name), "PerfStatus": None, "MaxFlipMean": maxFlipMean, "Flip": None, "Perf": None, "Command": subprocess.list2cmdline(command) }

        if name not in failures:
            result["ImageStatus"], result["Flip"] = EvaluateImage(testDir, os.path.join(SCENE_TESTS_DIR, name + ".png"), maxFlipMean)

            perf = AggregatePerf(LoadJson(os.path.join(testDir, PERF_FILE)))
            baseline = ComputeBaseline(previousRuns, runInfo, name, args.baseline_runs)
            result["Perf"] = perf
            result["PerfStatus"], result["PerfRegressions"] = EvaluatePerf(perf, baseline, args)
            result["MemoryChanges"] = EvaluateMemory(perf, baseline, testDir, name)
            result["PerfBaseline"] = baseline

            result["Settings"] = LoadJson(os.path.join(testDir, SETTINGS_FILE))
            result["SettingsChanges"] = EvaluateSettings(result["Settings"], os.path.join(SCENE_TESTS_DIR, name + GOLDEN_SETTINGS_SUFFIX))
            result["PipelineStatisticsChanges"] = EvaluatePipelineStatistics(perf, os.path.join(SCENE_TESTS_DIR, name + GOLDEN_PIPELINE_STATISTICS_SUFFIX))
            result["WorkloadChanges"] = EvaluateWorkload(perf, os.path.join(SCENE_TESTS_DIR, name + GOLDEN_WORKLOAD_SUFFIX))
            result["PassHashes"] = EvaluatePassHashes(testDir, os.path.join(SCENE_TESTS_DIR, name + GOLDEN_HASHES_SUFFIX))
            if result["PassHashes"]:
                result["PassHashes"]["ScopeStatus"], result["PassHashes"]["ScopeMismatches"] = MapPassHashesToScopes(result["PassHashes"], perf)

        runInfo["Tests"][name] = result

        flipText = "{:.5f}".format(result["Flip"]["Mean"]) if result["Flip"] else "-"
        frameText = "min {MinMs:.3f} / median {Ms:.3f} / p90 {P90Ms:.3f} ms".format(**result["Perf"]["GPUFrame"]) if result["Perf"] else "-"
        hashes = result.get("PassHashes")
        hashesText = "{}/{} passes match, first mismatch: {}".format(hashes["Matched"], len(hashes["Passes"]), hashes["FirstMismatch"] or "-") if hashes else "no golden hashes"
        settingsChanges = result.get("SettingsChanges")
        settingsText = "{} setting(s) changed".format(len(settingsChanges)) if settingsChanges is not None else "no golden settings"
        workloadChanges = result.get("WorkloadChanges")
        workloadText = "{} pass(es) workload changed".format(len(workloadChanges)) if workloadChanges is not None else "no golden workload"
        memoryChanges = result.get("MemoryChanges")
        memoryText = "{} memory change(s) vs previous run".format(MemoryChangesNum(memoryChanges)) if memoryChanges is not None else "no memory baseline"
        print("[{}] image: {} (FLIP {}), perf: {} ({}), {}, {}, {}, {}".format(name, result["ImageStatus"], flipText, result["PerfStatus"] or "-", frameText, hashesText, settingsText, workloadText, memoryText))


def CommandRun(args):
    startTime = time.perf_counter()

    testPaths = DiscoverTests(args.filter)
    if not testPaths:
        sys.exit("No tests matching '{}' in {}".format(args.filter, SCENE_TESTS_DIR))

    previousRuns = LoadRuns()

    runId = datetime.datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    runDir = os.path.join(RUNS_DIR, runId)
    os.makedirs(runDir)

    runInfo = {
        "Id": runId,
        "Date": datetime.datetime.now().isoformat(timespec="seconds"),
        "Commit": Git("rev-parse", "--short", "HEAD"),
        "Dirty": bool(Git("status", "--porcelain", "--untracked-files=no")),
        "Label": args.label,
        "Mode": args.mode,
        "Config": args.config,
        "Machine": platform.node(),
        "RayTracing": not args.no_ray_tracing,
        "Tests": {},
    }

    failures = ExecuteTests(args, runDir, testPaths)
    EvaluateRun(args, runDir, runInfo, testPaths, failures, previousRuns)

    SaveJson(os.path.join(runDir, "Run.json"), runInfo)
    GenerateReport(previousRuns + [runInfo])
    if not args.no_open:
        OpenReport()

    results = runInfo["Tests"].values()
    failed = [result for result in results if result["ImageStatus"] not in ("Pass", "NoGolden") or result["PerfStatus"] == "Regression"]
    print("{} test(s), {} failed, total time: {:.1f} s. Report: {}".format(len(results), len(failed), time.perf_counter() - startTime, REPORT_PATH))
    return 1 if failed else 0


# Accept ============================================================================================

def CommandAccept(args):
    runs = LoadRuns()
    run = next((run for run in runs if run["Id"] == args.run), None) if args.run else (runs[-1] if runs else None)
    if run is None:
        sys.exit("Run not found")

    names = list(args.tests)
    if args.failed:
        names += [name for name, result in run["Tests"].items() if result["ImageStatus"] in ("Fail", "NoGolden")]
    if args.passed:
        names += [name for name, result in run["Tests"].items() if result["ImageStatus"] == "Pass"]

    for name in names:
        resultPath = os.path.join(RUNS_DIR, run["Id"], name, "Result.png")
        if not os.path.exists(resultPath):
            print("[{}] no result image in run {}, skipped".format(name, run["Id"]))
            continue
        shutil.copyfile(resultPath, os.path.join(SCENE_TESTS_DIR, name + ".png"))
        shutil.copyfile(os.path.join(RUNS_DIR, run["Id"], name, HASHES_FILE), os.path.join(SCENE_TESTS_DIR, name + GOLDEN_HASHES_SUFFIX))
        shutil.copyfile(os.path.join(RUNS_DIR, run["Id"], name, SETTINGS_FILE), os.path.join(SCENE_TESTS_DIR, name + GOLDEN_SETTINGS_SUFFIX))
        scopes = run["Tests"][name]["Perf"]["Scopes"]
        pipelineStatistics = { path: scope["PipelineStatistics"] for path, scope in scopes.items() if "PipelineStatistics" in scope }
        SaveJson(os.path.join(SCENE_TESTS_DIR, name + GOLDEN_PIPELINE_STATISTICS_SUFFIX), pipelineStatistics)
        # All scopes are stored, so scopes without invocations are compared too
        SaveJson(os.path.join(SCENE_TESTS_DIR, name + GOLDEN_WORKLOAD_SUFFIX), { path: { key: scope[key] for key in WORKLOAD_KEYS if key in scope } for path, scope in scopes.items() })

        print("[{}] golden updated from run {}".format(name, run["Id"]))

    return 0


# Report ============================================================================================

def WritePerfSamples(runId, testName):
    """Per frame samples of every scope, loaded by the report on demand as a script, because browsers block fetch from file:// pages.
    Scopes with repeated paths (e.g. multiple dispatches of the same pass) are summed per frame."""
    testDir = os.path.join(RUNS_DIR, runId, testName)
    samplesPath = os.path.join(testDir, SAMPLES_FILE)
    if os.path.exists(samplesPath):
        return

    frames = LoadJson(os.path.join(testDir, PERF_FILE))["Frames"]
    samples = {}
    for frameIdx, frame in enumerate(frames):
        for scope in frame["Scopes"]:
            values = samples.setdefault(scope["Path"], [0.0] * len(frames))
            values[frameIdx] += scope["DurationMs"]
    samples["GPU Frame"] = [frame["GPUFrameMs"] for frame in frames]
    samples = { path: [round(v, 4) for v in values] for path, values in samples.items() }

    with open(samplesPath, "w") as file:
        file.write("perfSamples[{}] = {};".format(json.dumps(runId + "/" + testName), json.dumps(samples)))


def WriteTransientMemory(runId, testName):
    """[scope path, transient memory usage, allocations, deallocations] of every pass in execution order, from the last measured frame. Loaded by the report on demand, like samples.
    Events are [name, size, allocation id], allocations happen at the start of a pass and deallocations at its end, so both are included in its transient memory usage."""
    testDir = os.path.join(RUNS_DIR, runId, testName)
    memoryPath = os.path.join(testDir, TRANSIENT_MEMORY_FILE)
    if os.path.exists(memoryPath):
        return

    frame = LoadJson(os.path.join(testDir, PERF_FILE))["Frames"][-1]
    usages = [[scope["Path"], scope["TransientMemoryUsage"], *TransientEvents(scope, ("Name", "Size", "Id"))] for scope in frame["Scopes"] if "TransientMemoryUsage" in scope]

    with open(memoryPath, "w") as file:
        file.write("transientMemory[{}] = {};".format(json.dumps(runId + "/" + testName), json.dumps(usages)))


def GenerateReport(runs):
    with open(REPORT_TEMPLATE_PATH, "r", encoding="utf-8") as file:
        template = file.read()

    for run in runs:
        for name, result in run["Tests"].items():
            if result["Perf"]:
                WritePerfSamples(run["Id"], name)
                WriteTransientMemory(run["Id"], name)

    data = json.dumps(runs).replace("</", "<\\/")
    with open(REPORT_PATH, "w", encoding="utf-8") as file:
        file.write(template.replace("/*RUNS_DATA*/[]", data))


def OpenReport():
    if not webbrowser.open(pathlib.Path(REPORT_PATH).as_uri()):
        print("Failed to open browser, open report manually: {}".format(REPORT_PATH))


def CommandReport(args):
    GenerateReport(LoadRuns())
    print("Report: {}".format(REPORT_PATH))
    if not args.no_open:
        OpenReport()
    return 0


def main():
    parser = argparse.ArgumentParser(description="Sculptor render tests")
    subparsers = parser.add_subparsers(dest="command", required=True)

    runParser = subparsers.add_parser("run", help="Run tests, compare with goldens and track performance")
    runParser.add_argument("--mode", choices=["fast", "robust"], default="fast", help="fast: one engine instance for all tests, robust: engine instance per test")
    runParser.add_argument("--filter", default="*", help="Glob on test names")
    runParser.add_argument("--config", default="Development", choices=["Debug", "Development", "Release"])
    runParser.add_argument("--label", default="", help="Free text stored with the run")
    runParser.add_argument("--validation", action="store_true", help="Enable Vulkan validation")
    runParser.add_argument("--no-ray-tracing", action="store_true")
    runParser.add_argument("--timeout", type=int, default=300, help="Per test timeout in seconds")
    runParser.add_argument("--baseline-runs", type=int, default=5, help="Number of previous comparable runs used as performance baseline")
    runParser.add_argument("--perf-threshold", type=float, default=0.05, help="Relative slowdown treated as regression")
    runParser.add_argument("--perf-min-delta-ms", type=float, default=0.05, help="Minimal absolute slowdown treated as regression")
    runParser.add_argument("--no-open", action="store_true", help="Don't open report after run")
    runParser.set_defaults(func=CommandRun)

    acceptParser = subparsers.add_parser("accept", help="Copy results of a run to goldens")
    acceptParser.add_argument("tests", nargs="*")
    acceptParser.add_argument("--failed", action="store_true", help="Accept all failed and missing goldens")
    acceptParser.add_argument("--passed", action="store_true", help="Accept tests that passed")
    acceptParser.add_argument("--run", help="Run id (default: latest)")
    acceptParser.set_defaults(func=CommandAccept)

    reportParser = subparsers.add_parser("report", help="Regenerate HTML report")
    reportParser.add_argument("--no-open", action="store_true", help="Don't open report")
    reportParser.set_defaults(func=CommandReport)

    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
