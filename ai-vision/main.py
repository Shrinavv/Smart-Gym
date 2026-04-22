import cv2
import time
import requests
import numpy as np
from collections import deque
from ultralytics import YOLO

# --- Configuration (Local Mode) ---
MODEL_PATH = "yolo26n-pose.pt"
LOCAL_URL = "http://localhost:5000/update_vision"

MISTAKE_TIME_THRESHOLD = 0.4 
COOLDOWN_TIME = 0.5 # Reduced for local speed
ASYMMETRY_LIMIT = 50

model = YOLO(MODEL_PATH)
baseline_torso = None
mistake_timers = {"ID 1": 0, "ID 2": 0}
last_alert_time = 0
current_direction = "NONE"
last_wrist_y = None
time_window = 0.25 
y_history = deque()

def get_angle(hip, shoulder, elbow):
    v_torso = np.array([hip[0] - shoulder[0], hip[1] - shoulder[1]])
    v_arm = np.array([elbow[0] - shoulder[0], elbow[1] - shoulder[1]])
    mag_torso = np.linalg.norm(v_torso)
    mag_arm = np.linalg.norm(v_arm)
    if mag_torso == 0 or mag_arm == 0: return 0
    cos_angle = np.dot(v_torso, v_arm) / (mag_torso * mag_arm)
    return np.degrees(np.arccos(np.clip(cos_angle, -1.0, 1.0)))

def detect_asymmetry(kp, now):
    diff = abs(kp[9][1] - kp[10][1])
    if diff > ASYMMETRY_LIMIT:
        if mistake_timers["ID 1"] == 0: mistake_timers["ID 1"] = now
        if now - mistake_timers["ID 1"] > MISTAKE_TIME_THRESHOLD: return "ID 1"
    else: mistake_timers["ID 1"] = 0
    return None

def detect_posture_issue(kp, now):
    global baseline_torso
    s_y = (kp[5][1] + kp[6][1]) / 2
    h_y = (kp[11][1] + kp[12][1]) / 2
    current_torso = abs(s_y - h_y)
    if baseline_torso is None or baseline_torso < 10:
        baseline_torso = current_torso
        return None
    if current_torso < (baseline_torso * 0.75):
        if mistake_timers["ID 2"] == 0: mistake_timers["ID 2"] = now
        if now - mistake_timers["ID 2"] > MISTAKE_TIME_THRESHOLD: return "ID 2"
    else: mistake_timers["ID 2"] = 0
    return None

def detect_half_rep_angle(kp):
    global current_direction, last_wrist_y
    mid_shoulder = [(kp[5][0] + kp[6][0])/2, (kp[5][1] + kp[6][1])/2]
    mid_hip = [(kp[11][0] + kp[12][0])/2, (kp[11][1] + kp[12][1])/2]
    mid_elbow = [(kp[7][0] + kp[8][0])/2, (kp[7][1] + kp[8][1])/2]
    wrist_y = (kp[9][1] + kp[10][1])/2
    if last_wrist_y is None:
        last_wrist_y = wrist_y
        return None
    dy = wrist_y - last_wrist_y
    new_direction = current_direction
    if dy > 3: new_direction = "DOWN"
    elif dy < -3: new_direction = "UP"
    result = None
    if new_direction != current_direction and current_direction != "NONE":
        angle = get_angle(mid_hip, mid_shoulder, mid_elbow)
        if 65 <= angle <= 120: result = "ID 3"
    current_direction = new_direction
    last_wrist_y = wrist_y
    return result

def detect_jerk_normalized(kp, now):
    wrist_y = (kp[9][1] + kp[10][1]) / 2
    s_y = (kp[5][1] + kp[6][1]) / 2
    h_y = (kp[11][1] + kp[12][1]) / 2
    torso_length = abs(s_y - h_y)
    y_history.append((now, wrist_y))
    while y_history and now - y_history[0][0] > time_window: y_history.popleft()
    if len(y_history) < 2 or torso_length == 0: return None
    y_vals = [y for t, y in y_history]
    if ((max(y_vals) - min(y_vals)) / torso_length) > 0.60: return "ID 4"
    return None

def send_to_local(mistakes):
    global last_alert_time
    if not mistakes or (time.time() - last_alert_time < COOLDOWN_TIME): return
    priority = {"ID 4": 4, "ID 1": 1, "ID 2": 2, "ID 3": 3}
    target_id = priority[sorted(mistakes, key=lambda x: priority.get(x, 99))[0]]
    try:
        requests.post(LOCAL_URL, json={'ai_state': target_id}, timeout=0.1)
        last_alert_time = time.time()
    except: pass

def main():
    cap = cv2.VideoCapture(0)
    print("=== Smart Gym AI Vision: LOCAL MODE ===")
    while cap.isOpened():
        success, frame = cap.read()
        if not success: break
        now = time.time()
        results = model.predict(frame, conf=0.5, verbose=False)
        current_mistakes = []
        for r in results:
            if not r.keypoints or len(r.keypoints.xy) == 0: continue
            kp = r.keypoints.xy[0].cpu().numpy()
            if kp[9][1] == 0 or kp[11][1] == 0 or kp[7][1] == 0: continue 
            m1 = detect_asymmetry(kp, now); m2 = detect_posture_issue(kp, now)
            m3 = detect_half_rep_angle(kp); m4 = detect_jerk_normalized(kp, now)
            for m in [m1, m2, m3, m4]:
                if m: current_mistakes.append(m)
        send_to_local(current_mistakes)
        cv2.imshow("Smart Gym - Monitor", results[0].plot())
        if cv2.waitKey(1) & 0xFF == ord('q'): break
    cap.release()
    cv2.destroyAllWindows()

if __name__ == "__main__": main()