document.addEventListener('DOMContentLoaded', () => {
    const historyBody = document.getElementById('historyBody');
    const refreshBtn = document.getElementById('refreshBtn');
    const dateFilter = document.getElementById('dateFilter');
    
    // Modal elements
    const modal = document.getElementById('imageModal');
    const modalImg = document.getElementById('modalImg');
    const modalCaption = document.getElementById('modalCaption');
    const closeBtn = document.querySelector('.close-btn');

    // Cache to prevent UI jitter on unchanged data
    let lastHistoryJson = "";

    // Fetch history from API
    const fetchHistory = async () => {
        try {
            refreshBtn.style.opacity = '0.5';
            const dateQuery = dateFilter.value ? `?date=${dateFilter.value}` : '';
            const response = await fetch(`/api/history${dateQuery}`);
            const data = await response.json();
            
            const currentHistoryJson = JSON.stringify(data);
            if (currentHistoryJson !== lastHistoryJson) {
                renderHistory(data);
                lastHistoryJson = currentHistoryJson;
            }
        } catch (error) {
            console.error("Error fetching history:", error);
            historyBody.innerHTML = `<tr><td colspan="6" class="text-center loading-text error">Failed to load data. Ensure server is running.</td></tr>`;
        } finally {
            refreshBtn.style.opacity = '1';
        }
    };

    // Render history table
    const renderHistory = (data) => {
        if (data.length === 0) {
            historyBody.innerHTML = `<tr><td colspan="6" class="text-center loading-text">No scans recorded yet. Trigger the ESP32-CAM to see logs.</td></tr>`;
            return;
        }

        historyBody.innerHTML = '';
        data.forEach(item => {
            const tr = document.createElement('tr');
            
            // Format confidence
            const confidenceStr = item.confidence > 0 ? `${item.confidence.toFixed(1)}%` : '-';
            
            // Badge class
            let badgeClass = 'error';
            if (item.status === 'success') badgeClass = 'success';
            if (item.status === 'failed') badgeClass = 'failed';

            tr.innerHTML = `
                <td>#${item.id}</td>
                <td>${item.timestamp}</td>
                <td>
                    <img src="${item.image_path}" class="thumb" alt="Scan" 
                         onerror="this.src='data:image/svg+xml;utf8,<svg xmlns=\\\'http://www.w3.org/2000/svg\\\' width=\\\'60\\\' height=\\\'45\\\'><rect width=\\\'60\\\' height=\\\'45\\\' fill=\\\'#333\\\'/><text x=\\\'30\\\' y=\\\'27\\\' fill=\\\'#fff\\\' font-size=\\\'10\\\' text-anchor=\\\'middle\\\'>No Img</text></svg>'"
                         onclick="openModal('${item.image_path}', '${item.message} - Confidence: ${confidenceStr}')">
                </td>
                <td><span class="badge ${badgeClass}">${item.status}</span></td>
                <td>${confidenceStr}</td>
                <td>${item.latency ? item.latency + 'ms' : '-'}</td>
                <td>${item.message}</td>
            `;
            historyBody.appendChild(tr);
        });
    };

    // Modal Logic
    window.openModal = (imgSrc, caption) => {
        modalImg.src = imgSrc;
        modalCaption.textContent = caption;
        modal.classList.add('show');
    };

    const closeModal = () => {
        modal.classList.remove('show');
        setTimeout(() => { modalImg.src = ''; }, 300); // Clear after animation
    };

    closeBtn.addEventListener('click', closeModal);
    modal.addEventListener('click', (e) => {
        if (e.target === modal) closeModal();
    });

    // Fetch status
    const fetchStatus = async () => {
        try {
            const startTime = performance.now();
            const response = await fetch('/api/status');
            const endTime = performance.now();
            const latencyMs = Math.round(endTime - startTime);
            
            const data = await response.json();
            
            const serverEl = document.getElementById('serverStatus');
            const esp32El = document.getElementById('esp32Status');
            
            if (data.server_status.includes('Online')) {
                serverEl.innerHTML = `<span class="pulse" style="background-color: #2ecc71;"></span> Server: ${data.server_status} (${latencyMs}ms)`;
            } else {
                serverEl.innerHTML = `<span class="pulse" style="background-color: #e74c3c;"></span> Server: Offline`;
            }
            
            if (data.esp32_cam_status === 'Online') {
                esp32El.innerHTML = `<span class="pulse" style="background-color: #2ecc71;"></span> ESP32: Online`;
            } else {
                esp32El.innerHTML = `<span class="pulse" style="background-color: #e74c3c;"></span> ESP32: Offline`;
            }
        } catch (error) {
            console.error("Error fetching status:", error);
            document.getElementById('serverStatus').innerHTML = `<span class="pulse" style="background-color: #e74c3c;"></span> Server: Error`;
            document.getElementById('esp32Status').innerHTML = `<span class="pulse" style="background-color: #e74c3c;"></span> ESP32: Unknown`;
        }
    };

    // Fetch Dates
    const fetchDates = async () => {
        try {
            const response = await fetch('/api/history/dates');
            const dates = await response.json();
            
            // Keep current selection
            const currentSelection = dateFilter.value;
            
            let html = '<option value="">All Days</option>';
            dates.forEach(date => {
                html += `<option value="${date}">${date}</option>`;
            });
            dateFilter.innerHTML = html;
            
            if (dates.includes(currentSelection)) {
                dateFilter.value = currentSelection;
            }
        } catch (error) {
            console.error("Error fetching dates:", error);
        }
    };

    // Initial fetch
    fetchDates();
    fetchHistory();
    fetchStatus();

    // Event listeners
    dateFilter.addEventListener('change', () => {
        lastHistoryJson = ""; // Force re-render
        fetchHistory();
    });

    refreshBtn.addEventListener('click', () => {
        fetchDates();
        fetchHistory();
        fetchStatus();
    });

    // Auto refresh every 5 seconds
    setInterval(() => {
        fetchHistory();
        fetchStatus();
    }, 5000);
});
