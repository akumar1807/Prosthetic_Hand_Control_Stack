import cv2 as cv
import mediapipe as mp
import time
import math as m

vid = cv.VideoCapture(0)

# Initialize MediaPipe Hands
mp_hands = mp.solutions.hands
hands = mp_hands.Hands(
    static_image_mode=False,
    max_num_hands=1, # Tracking 1 hand for clearer measurements
    min_detection_confidence=0.5,
    min_tracking_confidence=0.5
)
mp_draw = mp.solutions.drawing_utils

# Variables for FPS tracking
p_time = 0

while True:
    success, frame = vid.read()
    
    # Safety check to prevent crashing on empty frames
    if not success:
        print("Ignoring empty camera frame.")
        continue
        
    # Convert BGR (OpenCV) to RGB (MediaPipe)
    frame_rgb = cv.cvtColor(frame, cv.COLOR_BGR2RGB)
    result = hands.process(frame_rgb)
    h, w, c = frame.shape
    
    if result.multi_hand_landmarks:
        for hand_lms in result.multi_hand_landmarks:
            # Draw all the base hand connections
            mp_draw.draw_landmarks(frame, hand_lms, mp_hands.HAND_CONNECTIONS)
            
            # Extract coordinates for the 4 specific landmarks we need
            # 2 = Thumb MCP, 4 = Thumb Tip
            # 5 = Index MCP, 8 = Index Tip
            x2 = int(hand_lms.landmark[2].x * w)
            y2 = int(hand_lms.landmark[2].y * h)
            
            x4 = int(hand_lms.landmark[4].x * w)
            y4 = int(hand_lms.landmark[4].y * h)
            
            x5 = int(hand_lms.landmark[5].x * w)
            y5 = int(hand_lms.landmark[5].y * h)
            
            x8 = int(hand_lms.landmark[8].x * w)
            y8 = int(hand_lms.landmark[8].y * h)
            
            # Calculate 2D distance between Thumb tip (4) and Index tip (8)
            dist = m.hypot(x4 - x8, y4 - y8)
            
            # Draw a green line between the fingertips for distance
            cv.line(frame, (x4, y4), (x8, y8), [0, 255, 0], 3)
            
            # Draw magenta lines from knuckles to fingertips for the angle vectors
            cv.line(frame, (x2, y2), (x4, y4), [255, 0, 255], 3)
            cv.line(frame, (x5, y5), (x8, y8), [255, 0, 255], 3)
            
            # Vector magnitude (lengths of the lines)
            l1 = m.hypot(x4 - x2, y4 - y2)
            l2 = m.hypot(x8 - x5, y8 - y5)
            
            # Vector components
            v1_x, v1_y = (x4 - x2), (y4 - y2)
            v2_x, v2_y = (x8 - x5), (y8 - y5)
            
            # Prevent Division by Zero if landmarks overlap exactly
            if l1 > 0 and l2 > 0:
                # Dot product calculation
                dot_product = (v1_x * v2_x) + (v1_y * v2_y)
                inverse = dot_product / (l1 * l2)
                
                # Clamp the value between -1 and 1 to prevent math domain errors from floating point noise
                inverse = max(min(inverse, 1.0), -1.0) 
                
                angle = m.degrees(m.acos(inverse))
            else:
                angle = 0
            
            # Display Angle and Distance measurements
            cv.putText(frame, f'Angle: {int(angle)} deg', (30, 100), cv.FONT_HERSHEY_PLAIN, 2, [255, 0, 255], 3)
            cv.putText(frame, f'Distance: {int(dist)} px', (30, 150), cv.FONT_HERSHEY_PLAIN, 2, [0, 255, 0], 3)

    c_time = time.time()
    # Prevent division by zero on the very first frame
    if c_time - p_time > 0:
        fps = 1 / (c_time - p_time)
    else:
        fps = 0
    p_time = c_time
    
    cv.putText(frame, f'FPS: {int(fps)}', (30, 50), cv.FONT_HERSHEY_PLAIN, 2, [0, 0, 255], 3)
   
    cv.imshow("Combined Hand Tracker", frame)
    
    # Wait 1ms and check if the 'q' key is pressed to quit
    if cv.waitKey(1) & 0xFF == ord('q'):
        break

# Clean up resources
vid.release()
cv.destroyAllWindows()