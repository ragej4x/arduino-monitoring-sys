let phChart = null;
let ppmChart = null;
let firebaseConfig = {
  apiKey: "AIzaSyAOQlYeHmXBxVLXYcEHhgQrONLlGXTNQmQ",
  authDomain: "iot-monitoringsys.firebaseapp.com",
  databaseURL: "https://iot-monitoringsys-default-rtdb.asia-southeast1.firebasedatabase.app",
  projectId: "iot-monitoringsys",
  storageBucket: "iot-monitoringsys.appspot.com",
  messagingSenderId: "1069830261270",
  appId: "1:1069830261270:web:2c5f4e5b2e7d2e6f4b7c7e"
};

document.addEventListener('DOMContentLoaded', function() {
    firebase.initializeApp(firebaseConfig);
    
    initCharts();
    
    startFirebaseListeners();
    
    setupEventListeners();
    
    loadLogs();
});

function initCharts() {
    const phCtx = document.getElementById('phChart').getContext('2d');
    phChart = new Chart(phCtx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: 'pH Level',
                data: [],
                borderColor: 'rgba(75, 192, 192, 1)',
                backgroundColor: 'rgba(75, 192, 192, 0.2)',
                borderWidth: 2,
                tension: 0.4,
                fill: true
            }, {
                label: 'Target pH',
                data: [],
                borderColor: 'rgba(255, 99, 132, 1)',
                borderWidth: 2,
                borderDash: [5, 5],
                fill: false,
                pointRadius: 0
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                title: {
                    display: true,
                    text: 'pH Level History'
                }
            },
            scales: {
                y: {
                    beginAtZero: false,
                    suggestedMin: 5,
                    suggestedMax: 8
                }
            }
        }
    });
    
    const ppmCtx = document.getElementById('ppmChart').getContext('2d');
    ppmChart = new Chart(ppmCtx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: 'PPM Level',
                data: [],
                borderColor: 'rgba(153, 102, 255, 1)',
                backgroundColor: 'rgba(153, 102, 255, 0.2)',
                borderWidth: 2,
                tension: 0.4,
                fill: true
            }, {
                label: 'Target PPM',
                data: [],
                borderColor: 'rgba(255, 159, 64, 1)',
                borderWidth: 2,
                borderDash: [5, 5],
                fill: false,
                pointRadius: 0
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                title: {
                    display: true,
                    text: 'PPM Level History'
                }
            },
            scales: {
                y: {
                    beginAtZero: false
                }
            }
        }
    });
}

function startFirebaseListeners() {
    firebase.database().ref('/sensors').on('value', (snapshot) => {
        const data = snapshot.val();
        if (data) {
            updateSensorValues({
                temperature: data.temperature,
                ph: data.pH,
                ppm: data.PPM,
                water_level: data.waterLevel === "HIGH"
            });
            
            const timestamp = new Date().toLocaleString();
            document.getElementById('last-updated').textContent = timestamp;
            
            highlightUpdates();
            
            addDataPoint(data.pH, data.PPM);
        }
    });
    
    firebase.database().ref('/pumps').on('value', (snapshot) => {
        const data = snapshot.val();
        if (data) {
            updatePumpStatus({
                water_pump: data.water_pump,
                solution_a: data.pump_a,
                solution_b: data.pump_b,
                solution_c: data.pump_c,
                solution_d: data.pump_d
            });
        }
    });
    
    firebase.database().ref('/config').on('value', (snapshot) => {
        const data = snapshot.val();
        if (data) {
            document.getElementById('ph_min').value = data.ph_min;
            document.getElementById('current-ph-min').textContent = data.ph_min;
            
            document.getElementById('ph_limit').value = data.ph_limit;
            document.getElementById('current-ph-limit').textContent = data.ph_limit;
            
            document.getElementById('ppm_min').value = data.ppm_min;
            document.getElementById('current-ppm-min').textContent = data.ppm_min;
            
            document.getElementById('ppm_limit').value = data.ppm_limit;
            document.getElementById('current-ppm-limit').textContent = data.ppm_limit;
            
            document.getElementById('scan_interval').value = data.scan_interval;
            document.getElementById('current-scan-interval').textContent = data.scan_interval;
        }
    });
}

function addDataPoint(ph, ppm) {
    const now = new Date();
    const timeString = now.toLocaleTimeString();
    
    if (phChart.data.labels.length >= 20) {
        phChart.data.labels.shift();
        phChart.data.datasets[0].data.shift();
        phChart.data.datasets[1].data.shift();
    }
    phChart.data.labels.push(timeString);
    phChart.data.datasets[0].data.push(parseFloat(ph));
    
    const phLimit = parseFloat(document.getElementById('ph_limit').value);
    phChart.data.datasets[1].data.push(phLimit);
    phChart.update();
    
    if (ppmChart.data.labels.length >= 20) {
        ppmChart.data.labels.shift();
        ppmChart.data.datasets[0].data.shift();
        ppmChart.data.datasets[1].data.shift();
    }
    ppmChart.data.labels.push(timeString);
    ppmChart.data.datasets[0].data.push(parseInt(ppm));
    
    const ppmLimit = parseInt(document.getElementById('ppm_limit').value);
    ppmChart.data.datasets[1].data.push(ppmLimit);
    ppmChart.update();
}

function loadLogs() {
    firebase.database().ref('/logs').orderByKey().limitToLast(30).on('value', (snapshot) => {
        const logs = snapshot.val();
        if (!logs) return;
        
        const logsTableBody = document.getElementById('logs-table-body');
        logsTableBody.innerHTML = '';
        
        const logEntries = Object.entries(logs).sort((a, b) => b[0] - a[0]);
        
        logEntries.forEach(([timestamp, log]) => {
            const row = document.createElement('tr');
            
            const timeCell = document.createElement('td');
            const date = new Date(parseInt(timestamp));
            timeCell.textContent = date.toLocaleString();
            
            const messageCell = document.createElement('td');
            messageCell.textContent = log.message;
            
            row.appendChild(timeCell);
            row.appendChild(messageCell);
            logsTableBody.appendChild(row);
        });
    });
}

function updateSensorValues(sensors) {
    if (!sensors) return;
    
    if (sensors.temperature !== undefined) {
        const tempElement = document.getElementById('temp-value');
        tempElement.textContent = parseFloat(sensors.temperature).toFixed(1);
        updateValueClass(tempElement, sensors.temperature, 15, 25, 30);
    }
    
    if (sensors.ph !== undefined) {
        const phElement = document.getElementById('ph-value');
        phElement.textContent = parseFloat(sensors.ph).toFixed(2);
        
        const phMin = parseFloat(document.getElementById('ph_min').value);
        const phMax = parseFloat(document.getElementById('ph_limit').value);
        updateValueClass(phElement, sensors.ph, phMin, (phMin + phMax) / 2, phMax);
    }
    
    if (sensors.ppm !== undefined) {
        const ppmElement = document.getElementById('ppm-value');
        ppmElement.textContent = sensors.ppm;
        
        const ppmMin = parseInt(document.getElementById('ppm_min').value);
        const ppmMax = parseInt(document.getElementById('ppm_limit').value);
        updateValueClass(ppmElement, sensors.ppm, ppmMin, (ppmMin + ppmMax) / 2, ppmMax);
    }
    
    if (sensors.water_level !== undefined) {
        const waterLevelElement = document.getElementById('water-level');
        waterLevelElement.textContent = sensors.water_level ? "OK" : "LOW";
        waterLevelElement.className = sensors.water_level ? "value value-normal" : "value value-danger";
    }
}

function updatePumpStatus(pumps) {
    if (!pumps) return;
    
    updateSinglePumpStatus('water_pump', pumps.water_pump);
    updateSinglePumpStatus('solution_a', pumps.solution_a);
    updateSinglePumpStatus('solution_b', pumps.solution_b);
    updateSinglePumpStatus('solution_c', pumps.solution_c);
    updateSinglePumpStatus('solution_d', pumps.solution_d);
}

function updateSinglePumpStatus(pumpName, isOn) {
    if (isOn === undefined) return;
    
    const statusElement = document.getElementById(`${pumpName.replace('_', '-')}-status`);
    if (statusElement) {
        statusElement.textContent = isOn ? 'ON' : 'OFF';
        statusElement.className = isOn ? 'badge bg-success' : 'badge bg-secondary';
        
        const buttonContainer = statusElement.closest('.pump-status');
        if (buttonContainer) {
            const button = buttonContainer.querySelector('button');
            const actionInput = buttonContainer.querySelector('input[name="action"]');
            
            if (button && actionInput) {
                button.textContent = isOn ? 'Turn OFF' : 'Turn ON';
                button.className = isOn ? 'btn btn-sm btn-danger' : 'btn btn-sm btn-success';
                actionInput.value = isOn ? 'off' : 'on';
            }
        }
    }
}

function setupEventListeners() {
    const targetForm = document.getElementById('target-form');
    if (targetForm) {
        targetForm.addEventListener('submit', function(e) {
            e.preventDefault();
            
            const phMin = parseFloat(document.getElementById('ph_min').value);
            const phLimit = parseFloat(document.getElementById('ph_limit').value);
            const ppmMin = parseInt(document.getElementById('ppm_min').value);
            const ppmLimit = parseInt(document.getElementById('ppm_limit').value);
            const scanInterval = parseInt(document.getElementById('scan_interval').value);
            
            firebase.database().ref('/config').update({
                ph_min: phMin,
                ph_limit: phLimit,
                ppm_min: ppmMin,
                ppm_limit: ppmLimit,
                scan_interval: scanInterval
            }).then(() => {
                alert('Target values updated successfully!');
            }).catch(error => {
                console.error('Error updating target values:', error);
                alert('Error updating target values. Please try again.');
            });
        });
    }
    
    const pumpForms = document.querySelectorAll('.pump-control-form');
    pumpForms.forEach(form => {
        form.addEventListener('submit', function(e) {
            e.preventDefault();
            
            const pump = this.querySelector('input[name="pump"]').value;
            const action = this.querySelector('input[name="action"]').value;
            const isOn = action === 'on';
            
            const pumpMap = {
                'water_pump': 'water_pump',
                'solution_a': 'pump_a',
                'solution_b': 'pump_b',
                'solution_c': 'pump_c',
                'solution_d': 'pump_d'
            };
            
            const updates = {};
            updates[pumpMap[pump]] = isOn;
            
            if (pump === 'water_pump') {
                updates['water_pump_manual_control'] = true;
            }
            
            firebase.database().ref('/pumps').update(updates).catch(error => {
                console.error('Error controlling pump:', error);
                alert('Error controlling pump. Please try again.');
            });
        });
    });
}

function updateValueClass(element, value, lowThreshold, normalThreshold, highThreshold) {
    element.classList.remove('value-normal', 'value-warning', 'value-danger');
    
    if (value < lowThreshold) {
        element.classList.add('value-danger');
    } else if (value > highThreshold) {
        element.classList.add('value-danger');
    } else if (value < normalThreshold || value > normalThreshold) {
        element.classList.add('value-warning');
    } else {
        element.classList.add('value-normal');
    }
}

function highlightUpdates() {
    const elements = document.querySelectorAll('.value');
    
    elements.forEach(element => {
        element.classList.add('pulse');
        setTimeout(() => {
            element.classList.remove('pulse');
        }, 1500);
    });
}
