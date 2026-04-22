// web-dashboard/script.js
let myChart = null; 
let lastRepTime = Date.now();
let lastRepCount = 0;

async function fetchLocalData() {
  try {
    const res = await fetch("http://localhost:5000/api/data");
    return await res.json();
  } catch (error) {
    console.error("Local Server is offline");
    return null;
  }
}

// --- NEW/RESTORED: Rest Optimizer Logic ---
function runRestOptimizer(currentReps) {
    const restText = document.getElementById("rest-advice");
    
    // If reps increased, reset the rest timer
    if (currentReps > lastRepCount) {
        lastRepTime = Date.now();
        lastRepCount = currentReps;
        restText.innerText = "Set in progress... Keep going!";
        restText.style.color = "var(--accent)";
    } else if (currentReps === 0 && lastRepCount > 0) {
        // User finished a set
        const secondsSinceLastRep = Math.floor((Date.now() - lastRepTime) / 1000);
        
        if (secondsSinceLastRep < 60) {
            restText.innerText = `Resting: ${secondsSinceLastRep}s (Target: 60-90s)`;
            restText.style.color = "var(--warning)";
        } else {
            restText.innerText = "Recovery optimal. Ready for next set!";
            restText.style.color = "var(--good)";
        }
    }
}

function updateAICoach(mFeeds) {
  if (!mFeeds || mFeeds.length === 0) return;
  const latest = mFeeds[mFeeds.length - 1];
  
  const stateId = parseInt(latest.field4);
  const titleEl = document.getElementById("ai-status-title");
  const textEl = document.getElementById("ai-feedback-text");
  const boxEl = document.querySelector(".coach-display");

  boxEl.className = "coach-display"; 

  const feedbackMap = {
    0: { t: "Perfect Form", d: "Excellent control. Maintain this tempo.", c: "state-good" },
    1: { t: "Asymmetry", d: "Balance your grip. Right side is lagging.", c: "state-warn" },
    2: { t: "Posture Alert", d: "Keep your back flat against the pad.", c: "state-warn" },
    3: { t: "Half-Rep", d: "Increase range of motion. Go deeper.", c: "state-warn" },
    4: { t: "Weight Jerking", d: "Too much momentum! Slow down.", c: "state-danger" }
  };

  const config = feedbackMap[stateId] || feedbackMap[0];
  titleEl.innerText = config.t;
  textEl.innerText = config.d;
  if(config.c) boxEl.classList.add(config.c);
}

function updateChart(mFeeds) {
  // FIXED: Changed ID to match your index.html 'motionChart'
  const canvas = document.getElementById('motionChart'); 
  if (!canvas || !mFeeds || mFeeds.length === 0) return;
  const ctx = canvas.getContext('2d');
  
  const visionData = mFeeds.slice(-15); // Show last 15 points
  const labels = visionData.map(f => f.created_at.split('T')[1].split('.')[0]);
  const formIds = visionData.map(f => parseInt(f.field4));

  if (myChart) myChart.destroy(); 
  myChart = new Chart(ctx, {
    type: 'line',
    data: {
      labels: labels,
      datasets: [{
        label: 'Form Error ID (0=Perfect)',
        data: formIds,
        borderColor: '#3498db',
        backgroundColor: 'rgba(52, 152, 219, 0.1)',
        fill: true,
        tension: 0.4
      }]
    },
    options: { 
        responsive: true,
        maintainAspectRatio: false,
        animation: false, 
        scales: { y: { min: 0, max: 4, ticks: { stepSize: 1 } } } 
    }
  });
}

async function loadDashboard() {
  const data = await fetchLocalData();
  if (!data) return;

  // Update Header Info
  document.getElementById("user-display").innerText = data.current.user_id;
  document.getElementById("rep-display").innerText = data.current.rep_count;

  // Run the logic blocks
  updateAICoach(data.feeds);
  updateChart(data.feeds);
  runRestOptimizer(data.current.rep_count); 
}

setInterval(loadDashboard, 500);
loadDashboard();