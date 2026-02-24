# Run Command:
# python trello_api_test.py

import os
import requests
from dotenv import load_dotenv
from datetime import datetime, timezone

load_dotenv()

API_KEY = os.getenv("TRELLO_API_KEY")
TOKEN = os.getenv("TRELLO_TOKEN")
BOARD_ID = os.getenv("TRELLO_BOARD_ID")

# ------------------- RGB Simulator -------------------
print("Fetching board data...\n")

# 1. Get all lists (mapping range of the Pico W potentiometer)
lists_url = f"https://api.trello.com/1/boards/{BOARD_ID}/lists?key={API_KEY}&token={TOKEN}"
lists_response = requests.get(lists_url)

if lists_response.status_code == 200:
    trello_lists = lists_response.json()
    print(f"Found {len(trello_lists)} lists, available for potentiometer switching:")
    for index, t_list in enumerate(trello_lists):
        print(f"  [{index}] {t_list['name']}")
        
    # List Traking
    selected_list = trello_lists[0] # Input from potentiometer
    print(f"\n---> Simulating potentiometer input: Currently tracking list '{selected_list['name']}'\n")
    
    # 2. Get all cards
    cards_url = f"https://api.trello.com/1/lists/{selected_list['id']}/cards?key={API_KEY}&token={TOKEN}"
    cards_response = requests.get(cards_url)
    
    if cards_response.status_code == 200:
        cards = cards_response.json()
        print(f"There are {len(cards)} cards in this list. Starting pressure value calculation...")
        
        # 3. Pressure Analysis
        total_pressure = 0
        now = datetime.now(timezone.utc)
        
        for card in cards:
            print(f"\n   Card: {card['name']}")
            due_date_str = card.get('due')
            
            if due_date_str:
                # Trello returns time with 'Z' (UTC time), convert to Python parseable format
                due_date_str = due_date_str.replace('Z', '+00:00')
                try:
                    due_date = datetime.fromisoformat(due_date_str)
                    time_diff = due_date - now
                    hours_left = time_diff.total_seconds() / 3600
                    
                    if hours_left < 0:
                        print(f"    Status: Overdue (Overdue by {-hours_left:.1f} hours) -> Pressure +20")
                        total_pressure += 20
                    elif hours_left <= 24:
                        print(f"    Status: Due within 24 hours! ({hours_left:.1f} hours remaining) -> Pressure +10")
                        total_pressure += 10
                    elif hours_left <= 24 * 7:
                        print(f"    Status: Due within 7 days. ({hours_left/24:.1f} days remaining) -> Pressure +5")
                        total_pressure += 5
                    else:
                        print(f"    Status: Due in more than 7 days. -> Pressure +2")
                        total_pressure += 2
                except ValueError:
                    print("    Status: Date parsing error -> Pressure +1")
                    total_pressure += 1
            else:
                print("    Status: No due date -> Pressure +1")
                total_pressure += 1
        
        print("\n" + "=" * 40)
        print(f"Total pressure score for the current list: {total_pressure}")
        
        # 4. Map the pressure to RGB output (0-255)
        # Assuming 50 points and above is solid red (maximum pressure)
        MAX_PRESSURE_THRESHOLD = 50.0 
        pressure_ratio = min(total_pressure / MAX_PRESSURE_THRESHOLD, 1.0)
        
        red_pwm = int(255 * pressure_ratio)
        green_pwm = int(255 * (1 - pressure_ratio))
        blue_pwm = 0 # Default is not to mix blue
        
        if len(cards) == 0:
             print("List is empty: LED will be set to Blue (Red: 0, Green: 0, Blue: 255)")
        else:
             print(f"Pico W Pin PWM Output Instructions:")
             print(f"   -> 🔴 GP13 (Red):   {red_pwm}")
             print(f"   -> 🟢 GP14 (Green): {green_pwm}")
             print(f"   -> 🔵 GP15 (Blue):  {blue_pwm}")
             
    else:
        print("❌ Failed to fetch cards.")
else:
    print("❌ Failed to fetch lists.")
