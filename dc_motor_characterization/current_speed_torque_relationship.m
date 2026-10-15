% =========================================================================
% MATLAB Automated Logger & Plotter (With Anti-Freeze Timeout Protection)
% =========================================================================
clear; clc; close all;

% Clean up any leftover locked serial ports from a previous Ctrl+C abort
if ~isempty( serialportfind )
    delete(serialportfind);
end

% ==========================================
% 1. SERIAL PORT CONFIGURATION (EDIT THIS!)
% ==========================================
portName = "COM3"; % Update to match your ESP32 port (e.g., "COM3" or "/dev/ttyUSB0")
baudRate = 115200;
outputFileName = "motor_characterization_data.csv";

disp("Available Serial Ports on your computer:");
disp(serialportlist("available"));

fprintf("Connecting to ESP32 on %s at %d baud...\n", portName, baudRate);
esp32 = serialport(portName, baudRate);
configureTerminator(esp32, "LF"); % Use "LF" so it never hangs waiting for "CR/LF" mismatch!
esp32.Timeout = 2;               % 2-second safety timeout on serial reads
flush(esp32);
pause(2);

% summaryData stores strictly ONE averaged row per unique load mass!
% Columns: [PWM, Load_kg, Torque_Nm, Current_A, Avg_Speed_RPM, Avg_Speed_RadPerSec]
summaryData = [];
currentMass_kg = 0.001; % Default starting mass[cite: 2]

fprintf("\nConnected! Ready to control ESP32 and log data.\n");

% ==========================================
% 2. INTERACTIVE CONTROL & LOGGING LOOP
% ==========================================
keepTesting = true;

while keepTesting
    fprintf("\n-------------------------------------------------------\n");
    fprintf("CURRENT LOAD SETTING: %.3f kg\n", currentMass_kg);
    fprintf("MATLAB -> ESP32 CONTROLLER MENU:\n");
    fprintf("  [1] Run 4-Second Load Test (Enter Current after test)\n");
    fprintf("  [2] Unwind Pulley for 4 Seconds (Same PWM in reverse)\n");
    fprintf("  [3] Finish, Save Unique Load CSV, and Plot Graphs\n");
    choice = input("Select an option (1, 2, or 3): ", 's');
    
    if strcmp(choice, '1')
        % --- OPTION 1: SET MASS, RUN 4s LIFT, THEN ENTER CURRENT ---
        massInput = input(sprintf("  Enter Load Mass in kg [Press Enter to keep %.3f kg]: ", currentMass_kg), 's');
        if ~isempty(strtrim(massInput))
            currentMass_kg = str2double(massInput);
        end
        
        % 1. Send mass to ESP32
        writeline(esp32, sprintf("%.3f", currentMass_kg));
        pause(0.2);
        flush(esp32);
        
        % 2. Start 4-second winding test
        fprintf("\n>>> Running 4-second lift... Watch your DC supply current reading!\n");
        fprintf("Time(s) | PWM | Load(kg) | Torque(Nm) | RPM | rad/s\n");
        writeline(esp32, "s");
        
        runBuffer = [];
        lastDataTime = tic; % Timer to detect when ESP32 finishes sending data
        hasStartedReceiving = false;
        
        while true
            if esp32.NumBytesAvailable > 0
                lineStr = strtrim(readline(esp32));
                lastDataTime = tic; % Reset idle timer every time a line arrives
                
                % Exit immediately if the ESP32 prints the END message
                if contains(lineStr, "END OF") || contains(lineStr, "STOPPED")
                    fprintf(">>> Run Complete! Motor stopped.\n");
                    break;
                end
                
                % Parse numeric CSV rows
                if ~startsWith(lineStr, "%") && strlength(lineStr) > 0
                    rowValues = str2num(lineStr); %#ok<ST2NM>
                    if length(rowValues) == 6
                        hasStartedReceiving = true;
                        runBuffer = [runBuffer; rowValues]; %#ok<AGROW>
                        fprintf("  %s\n", lineStr);
                    end
                end
            else
                % ANTI-FREEZE CHECK:
                % If data has started streaming and the ESP32 goes silent for > 0.8 seconds,
                % the 4-second test is done! Break out of the loop automatically.
                if hasStartedReceiving && toc(lastDataTime) > 0.8
                    fprintf(">>> Data stream finished (4.00s reached)! Motor stopped.\n");
                    break;
                end
                % Fallback timeout in case 's' was never received (6 seconds)
                if ~hasStartedReceiving && toc(lastDataTime) > 6.0
                    fprintf(">>> Timeout waiting for ESP32 response.\n");
                    break;
                end
                pause(0.01); % Brief pause so MATLAB doesn't lock up the CPU
            end
        end
        
        % 3. Take DC Supply Current as user input AFTER the test is done
        if ~isempty(runBuffer)
            observedCurrent_A = input("\n  Enter the Current (Amps) observed on your DC Supply: ");
            
            % Average steady-state rows (ignoring the first 0.5s acceleration transient)
            steadyMask = runBuffer(:, 1) >= 0.5;
            if ~any(steadyMask)
                steadyMask = true(size(runBuffer, 1), 1);
            end
            
            pwmVal     = runBuffer(1, 2);
            loadVal    = runBuffer(1, 3);
            torqueVal  = mean(runBuffer(steadyMask, 4));
            avgRPM     = mean(runBuffer(steadyMask, 5));
            avgRadSec  = mean(runBuffer(steadyMask, 6));
            
            newRow = [pwmVal, loadVal, torqueVal, observedCurrent_A, avgRPM, avgRadSec];
            
            % 4. OVERWRITE CHECK: If this mass already exists, overwrite its row!
            existingIdx = [];
            if ~isempty(summaryData)
                existingIdx = find(abs(summaryData(:, 2) - loadVal) < 1e-4, 1);
            end
            
            if ~isempty(existingIdx)
                summaryData(existingIdx, :) = newRow;
                fprintf(">>> Mass %.3f kg unchanged/already exists: OVERWROTE previous row.\n", loadVal);
            else
                summaryData = [summaryData; newRow]; %#ok<AGROW>
                fprintf(">>> Added new row for Load Mass = %.3f kg.\n", loadVal);
            end
            
            writematrix(summaryData, outputFileName);
        end
        
    elseif strcmp(choice, '2')
        % --- OPTION 2: AUTOMATIC 4-SECOND UNWIND (WITH TIMEOUT) ---
        fprintf("\n>>> Unwinding pulley at reverse targetPWM for 4 seconds...\n");
        flush(esp32);
        writeline(esp32, "r");
        
        unwindTimer = tic;
        while toc(unwindTimer) < 4.5 % Automatically completes after 4.5s even if text is missed
            if esp32.NumBytesAvailable > 0
                lineStr = strtrim(readline(esp32));
                if contains(lineStr, "END OF") || contains(lineStr, "STOPPED")
                    break;
                end
            end
            pause(0.05);
        end
        fprintf(">>> Unwind Complete! Motor stopped.\n");
        
    elseif strcmp(choice, '3')
        % --- OPTION 3: FINISH AND PLOT ---
        writeline(esp32, "x");
        keepTesting = false;
    else
        fprintf("Invalid selection. Please type 1, 2, or 3.\n");
    end
end

clear esp32;
fprintf("\nSerial port closed.\n");

% ==========================================
% 3. SAVE FINAL CSV & PLOT RELATIONSHIPS
% ==========================================
if isempty(summaryData)
    fprintf("No data recorded. Exiting without plotting.\n");
    return;
end

summaryData = sortrows(summaryData, 2);

csvHeaders = {'PWM', 'Load_kg', 'Torque_Nm', 'Current_A', 'Speed_RPM', 'Speed_RadPerSec'};
outputTable = array2table(summaryData, 'VariableNames', csvHeaders);
writetable(outputTable, outputFileName);
fprintf("Saved %d unique load rows to '%s'!\n", size(summaryData, 1), outputFileName);
disp(outputTable);

avgTorque  = summaryData(:, 3);
avgCurrent = summaryData(:, 4);
avgRadSec  = summaryData(:, 6);

figure('Name', 'Motor Characterization Curves', 'Position', [100, 100, 900, 400]);

subplot(1, 2, 1);
plot(avgCurrent, avgRadSec, 'bo-', 'LineWidth', 2, 'MarkerSize', 8, 'MarkerFaceColor', 'b');
grid on;
xlabel('Supply Current (A)');
ylabel('Angular Velocity (rad/s)');
title('Speed - Current Relationship (\omega vs. I)');

subplot(1, 2, 2);
plot(avgCurrent, avgTorque, 'ro-', 'LineWidth', 2, 'MarkerSize', 8, 'MarkerFaceColor', 'r');
grid on;
xlabel('Supply Current (A)');
ylabel('Load Torque (N\cdotm)');
title('Torque - Current Relationship (\tau vs. I)');