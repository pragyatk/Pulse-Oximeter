# Importing necessary libraries for the server and data processing
from flask import Flask, request, jsonify
from scipy.signal import find_peaks
import numpy as np
import matplotlib.pyplot as plt

# Initializing the Flask application
app = Flask(__name__)

# Initialize global variables to store calculated values and counts
r_value = 0
heart_rates = [0, 0]
red_AC = None
red_DC = None
ir_AC = None
ir_DC = None
count = 0

# Route to handle POST requests for incoming PPG data
@app.route('/send_data', methods=['POST'])
def handle_data():
    global red_AC, red_DC, ir_AC, ir_DC, count  # Access global variables
    data = request.json  # Get JSON data sent with the POST request
    indicator = data["indicator"]  # Indicator to distinguish between red and infrared data
    data_string = data["dataString"]  # String containing the sensor data
    float_array = convert_to_array(data_string)  # Convert string data to a numpy array

    # Process red or infrared data based on the indicator
    if indicator == "red":
        red_AC, red_DC = find_ACDC(float_array, 0.7, 5.5, "red")  # Find AC/DC components for red
        print(f"RED AC: {red_AC}\nRED DC: {red_DC}")
        count += 1  # Increment the counter for received data sets
    elif indicator == "ir":
        ir_AC, ir_DC = find_ACDC(float_array, 0.7, 5.5, "ir")  # Find AC/DC components for infrared
        print(f"IR AC: {ir_AC}\nIR DC: {ir_DC}")
        count += 1

    # Once both red and IR data have been processed, calculate the R-value
    if count == 2:
        calculate_r(red_AC, red_DC, ir_AC, ir_DC)  # Calculate R-value from the two sets of data
        count = 0  # Reset the count after calculating R-value
    return f"Data processed", 200  # Respond to the client after processing data

# Helper function to convert the incoming data string to a numpy array
def convert_to_array(data_string):
    float_array = [float(item) for item in data_string.split(',')]  # Convert each item in the string to float
    for i in range(len(float_array)):
        float_array[i] = round(float_array[i], 5)  # Round to 5 decimal places
    float_array = np.array(float_array)  # Convert the list to a numpy array
    return float_array

# Function to find AC and DC components of the PPG signal using FFT and filtering techniques
def find_ACDC(array, hp_fc, lp_fc, indicator):
    global heart_rates  # Access global heart rate array
    length = len(array)
    t = np.linspace(0, length * 0.004, length)  # Generate time array
    trimmed_times = t[300:-500]  # Trim initial times to avoid startup and end-time transients
    trimmed_array = array[300:-500]  # Trim data array correspondingly
    array_fft = np.fft.fftshift(np.fft.fft(trimmed_array))  # Apply FFT and shift zero frequency to center
    freqs = np.linspace(-125, 125, len(array_fft))  # Frequency array

    # Create low-pass and high-pass filters
    lpf = np.zeros(len(freqs))
    lpf[(freqs >= -lp_fc) & (freqs <= lp_fc)] = 1  # Apply low-pass filter
    hpf = np.ones(len(freqs))
    hpf[(freqs >= -hp_fc) & (freqs <= hp_fc)] = 0  # Apply high-pass filter

    # Apply both filters to FFT data
    filtered_array_fft = lpf * hpf * array_fft
    filtered_array = np.fft.ifft(np.fft.fftshift(filtered_array_fft)) + 1.65  # Inverse FFT and apply offset correction
    filtered_array = np.abs(filtered_array)

    # Finding true peaks and troughs in the filtered signal using standard prominence
    peaks, _ = find_peaks(filtered_array, prominence=0.5 * np.std(filtered_array))
    peak_times = trimmed_times[peaks]
    troughs, _ = find_peaks(-filtered_array, prominence=0.5 * np.std(-filtered_array))

    # Calculate heart rate by determining the maximum interval between peaks
    peak_intervals = [peak_times[i + 1] - peak_times[i] for i in range(len(peak_times) - 1)]
    max_interval = max(peak_intervals)
    hr = 60 / max_interval
    index = indicator == "red"
    heart_rates[index] = hr  # Update heart rate based on the signal type

    # Calculate AC and DC values from peaks and troughs
    ac_values = []
    dc_values = []
    for i in range(len(troughs) - 1):
        slice = filtered_array[troughs[i] - 1:troughs[i + 1] + 1]  # Isolate each PPG slice
        dc_values.append(min(slice))  # Determine the DC component
        ac_values.append(max(slice) - min(slice))  # Calculate the AC component

    return ac_values, dc_values  # Return calculated AC and DC component arrays

# Plotting function for visualization and debugging
def plot_ppg(x, y, peaks, troughs, indicator):
    plt.figure()
    plt.plot(x, y, label='Filtered Signal')
    plt.plot(x[peaks], y[peaks], "x", label="Detected peaks")
    plt.plot(x[troughs], y[troughs], "x", label="Detected troughs")
    plt.ylim([0, 3.3])
    plt.legend()
    plt.savefig(f"{indicator}.png")  # Save plot to a file

# Function to calculate the R value using AC and DC components of red and IR light
def calculate_r(red_AC, red_DC, ir_AC, ir_DC):
    global r_value  # Access global R value

    # Ensure both arrays are of the same size, truncate if not
    if len(red_AC) != len(ir_AC):
        min_length = min(len(red_AC), len(ir_AC))
        red_AC = red_AC[:min_length]
        red_DC = red_DC[:min_length]
        ir_AC = ir_AC[:min_length]
        ir_DC = ir_DC[:min_length]

    # Calculate R value for each pair of AC/DC
    r_values = [(red_AC[i] / red_DC[i]) / (ir_AC[i] / ir_DC[i]) for i in range(len(red_AC))]
    r_std = np.std(r_values)
    filtered_r = [r for r in r_values if abs(r - np.mean(r_values)) < r_std]  # Filter outliers
    calculated_r = np.mean(filtered_r)  # Average R values to get the final R value
    r_value = calculated_r  # Update the global R value
    return

# Route to send the latest calculated R value to the client
@app.route('/retrieve_data', methods=['GET'])
def send_oxygenation_and_hr():
    global r_value, heart_rates
    # Calculate oxygen saturation using a quadratic formula with the R value
    oxygenation = -40.22 * r_value**2 + 24.67 * r_value + 96.06
    heart_rate = np.mean(heart_rates)  # Calculate mean heart rate
    return jsonify({"spo2": oxygenation, "hr": heart_rate}), 200  # Return oxygenation and heart rate as a JSON response

# Main function to run the Flask application
if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)  # Start the server on all available IPs at port 5000
