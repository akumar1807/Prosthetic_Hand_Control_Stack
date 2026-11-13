% =========================================================================
% MATLAB Automated Live-Tuning PID Logger
% =========================================================================
clear; clc; close all;

if ~isempty(serialportfind)
    delete(serialportfind);
end

% 1. SERIAL PORT CONFIGURATION
portName = "COM3"; % Update this to your active port
baudRate = 115200;

fprintf("Connecting to ESP32 on %s at %d baud...\n", portName, baudRate);
esp32 = serialport(portName, baudRate);
configureTerminator(esp32, "LF"); 
esp32.Timeout = 2; 
flush(esp32); 
pause(2); 

data_P   = []; data_PD  = []; data_PID = [];

fprintf("\nConnected! Ready to tune and evaluate controllers.\n");

% 2. INTERACTIVE LOGGING LOOP
keepTesting = true;

while keepTesting
    fprintf("\n-------------------------------------------------------\n");
    fprintf("LIVE TUNING & EVALUATION MENU:\n");
    fprintf("  [1] Tune & Record P Controller\n");
    fprintf("  [2] Tune & Record PD Controller\n");
    fprintf("  [3] Tune & Record PID Controller\n");
    fprintf("  [4] Finish, Evaluate Metrics, and Plot\n");
    choice = input("Select an option (1-4): ", 's');
    
    if ismember(choice, {'1', '2', '3'})
        
        % Prompt for live gains based on the controller type
        if strcmp(choice, '1')
            Kp = input("  Enter Kp: ");
            gainCmd = sprintf("g,%.3f,0.0,0.0", Kp);
        elseif strcmp(choice, '2')
            Kp = input("  Enter Kp: ");
            Kd = input("  Enter Kd: ");
            gainCmd = sprintf("g,%.3f,0.0,%.3f", Kp, Kd);
        elseif strcmp(choice, '3')
            Kp = input("  Enter Kp: ");
            Ki = input("  Enter Ki: ");
            Kd = input("  Enter Kd: ");
            gainCmd = sprintf("g,%.3f,%.3f,%.3f", Kp, Ki, Kd);
        end
        
        % Transmit gains to ESP32
        writeline(esp32, gainCmd);
        pause(0.2);
        flush(esp32); % Clear the ESP32 confirmation message from the buffer
        
        fprintf('\n>>> Running 2-second step response...\n');
        runBuffer = [];
        writeline(esp32, "s"); % Start command
        
        hasStartedReceiving = false;
        lastDataTime = tic; 
        
        while true
            if esp32.NumBytesAvailable > 0
                lineStr = strtrim(readline(esp32));
                lastDataTime = tic; 
                
                if contains(lineStr, "END") || contains(lineStr, "ERROR")
                    fprintf(">>> Run complete!\n");
                    break;
                end
                
                if strlength(lineStr) > 0 && ~startsWith(lineStr, "System")
                    rowValues = str2num(lineStr); %#ok<ST2NM>
                    if length(rowValues) == 3
                        hasStartedReceiving = true;
                        rowValues(1) = rowValues(1) / 1000.0; % Convert ms to seconds
                        runBuffer = [runBuffer; rowValues]; %#ok<AGROW>
                    end
                end
            else
                if hasStartedReceiving && toc(lastDataTime) > 0.8
                    fprintf(">>> Data stream finished!\n");
                    break;
                end
                if ~hasStartedReceiving && toc(lastDataTime) > 3.0
                    fprintf(">>> Timeout. ESP32 did not respond.\n");
                    break;
                end
                pause(0.01); 
            end
        end
        
        % Save to corresponding controller buffer (overwrites previous runs)
        if strcmp(choice, '1')
            data_P = runBuffer;
        elseif strcmp(choice, '2')
            data_PD = runBuffer;
        elseif strcmp(choice, '3')
            data_PID = runBuffer;
        end
        
    elseif strcmp(choice, '4')
        keepTesting = false;
    else
        fprintf("Invalid selection.\n");
    end
end

clear esp32; 

% 3. CALCULATE METRICS & PLOT (Unchanged from previous script)
figure('Name', 'Controller Tracking Performance', 'Position', [100, 100, 800, 500]);
hold on; grid on;
colors = {'r-', 'b-', 'g-'};
legendEntries = {};
target_angle = 90.0;

if ~isempty(data_P)
    info_P = stepinfo(data_P(:,3), data_P(:,1), target_angle);
    fprintf('\nP Controller:\n  Rise Time: %.3f s | Overshoot: %.1f%%\n', info_P.RiseTime, info_P.Overshoot);
    plot(data_P(:,1), data_P(:,3), colors{1}, 'LineWidth', 2);
    legendEntries{end+1} = 'P Control';
end

if ~isempty(data_PD)
    info_PD = stepinfo(data_PD(:,3), data_PD(:,1), target_angle);
    fprintf('PD Controller:\n  Rise Time: %.3f s | Overshoot: %.1f%%\n', info_PD.RiseTime, info_PD.Overshoot);
    plot(data_PD(:,1), data_PD(:,3), colors{2}, 'LineWidth', 2);
    legendEntries{end+1} = 'PD Control';
end

if ~isempty(data_PID)
    info_PID = stepinfo(data_PID(:,3), data_PID(:,1), target_angle);
    fprintf('PID Controller:\n  Rise Time: %.3f s | Overshoot: %.1f%%\n', info_PID.RiseTime, info_PID.Overshoot);
    plot(data_PID(:,1), data_PID(:,3), colors{3}, 'LineWidth', 2);
    legendEntries{end+1} = 'PID Control';
end

plot([0, 2.0], [target_angle, target_angle], 'k--', 'LineWidth', 1.5);
legendEntries{end+1} = 'Target Setpoint';

xlabel('Time (Seconds)', 'FontWeight', 'bold');
ylabel('Motor Angle (Degrees)', 'FontWeight', 'bold');
title('Free Motor Movement: Step Responses', 'FontWeight', 'bold');
legend(legendEntries, 'Location', 'southeast');
ylim([0, target_angle * 1.3]);