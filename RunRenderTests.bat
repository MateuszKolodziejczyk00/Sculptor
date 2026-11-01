@echo off
rem Runs render tests. Arguments are forwarded to RenderTests.py (e.g. "run --mode robust", "accept --failed", "report").
rem Without arguments runs all tests in fast mode.

if "%~1"=="" (
	python "%~dp0Source\Tools\RenderTests\Scripts\RenderTests.py" run
) else (
	python "%~dp0Source\Tools\RenderTests\Scripts\RenderTests.py" %*
)

exit /b %ERRORLEVEL%
