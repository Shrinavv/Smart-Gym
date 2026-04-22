# demo-simulator/simulator.py
import requests
import time
import random

URL = "http://localhost:5000/update_gym"

print("=== Hardware Simulator: Reps & Access Mode ===")

while True:
    user = random.choice(["343434345", "87654321", "11223344"])
    print(f"\nUser {user} detected at machine.")
    
    for rep in range(1, 11):
        requests.post(URL, json={"user_id": str(user), "rep_count": rep})
        print(f"   [IR SENSOR] Count: {rep}")
        time.sleep(3) # Simulating rep time
        
    print("Set finished. Resting...")
    requests.post(URL, json={"user_id": "Resting...", "rep_count": 0})
    time.sleep(10)