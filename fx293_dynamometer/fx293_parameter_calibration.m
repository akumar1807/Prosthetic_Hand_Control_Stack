% Applied Mass (grams) and Measured Voltages (mV)
mass = [0, 50, 100, 150, 200, 250]; 
voltage = [512.4, 520.1, 527.8, 535.4, 543.1, 550.9]; % Replace with your readings

p = polyfit(mass, voltage, 1);
S = p(1);   % Slope (mV/g)
V0 = p(2);  % Zero baseline (mV)

fprintf('Calibrated Sensitivity S  = %.4f mV/g\n', S);
fprintf('Calibrated Zero Baseline V0 = %.2f mV\n', V0);