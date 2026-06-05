@echo off
REM ============================================================
REM  Water Quality Monitoring System — Start Frontend & Backend (Dev Mode)
REM ============================================================

echo Starting Water Monitoring System (Dev Mode)...
echo.

REM Check if node_modules exists, if not install dependencies
if not exist "node_modules" (
    echo Installing dependencies...
    call npm install
    echo.
)

REM Start the backend server in dev mode with nodemon
echo Starting backend server in dev mode on port 3000...
start cmd /k "npm run dev"

REM Wait a moment for server to start
timeout /t 3 /nobreak

REM Open the frontend in default browser
echo Opening frontend in browser...
start http://localhost:3000

echo.
echo Water Monitoring System (Dev Mode) is running!
echo Backend: http://localhost:3000
echo Press Ctrl+C in the backend window to stop the server.
echo.
pause
