#include <Frame.h>
#include <LCDMap.h>
#include <Queue.h>
#include <LiquidCrystal.h>

// associate any needed LCD and joystick pins. SeedPin is used to generate a random seed.
const int rs = 1, en = 2, d4 = 3, d5 = 4, d6 = 5, d7 = 6, buttonPin = 7, xPin = A0, yPin = A1, seedPin = A5;

// system control
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);
LCDMap control(5, 8);
const int cellsCount = 32;                  // total number of cell slots on the screen
const int maxCustomChars = 8;               // maximum number of customer characters (due to memory)
bool cells[cellsCount];                     // keeps track of which cells are filled with custom character
const int frameRate = 15;                   // rate at which custom characters are re-drawn
const int deltaTime = 1000 / frameRate;     // time between frames
const int ERR = -9999;                      // error code

// joystick
const int minThreshL = 450;                 // lowest threshhold (lower bound) at which joystick should produce movement
const int minThreshH = 575;                 // lowest threshhold (higher bound)
const int maxThreshL = 12;                  // highest threshold (lower bound)
const int maxThreshH = 1000;                // lowest threshold (higher bound)
int joystickX;                              // joystick reading in X direction
int joystickY;                              // joystick reading in Y direction
int joystickButton;                         // joystick reading of button press

// fish control switch (player <-> computer AI)
bool playerControlled = true;               // true if fish is moved via joystick
int switchTimer = 3000;                     // if joystick is not used, wait this long to switch to AI movement
const int switchWait = 3000;                // for resetting timer

// fish
const char fish = 'F';                      // how LCDMap identifies fish
bool fishMoving = false;                    // true if fish is in motion
int fishSpeed = 0;                          // current fish speed (pixels / frame)
const int fishSpeedMin = 1;                 // minimum fish speed (pixels / frame)
const int fishSpeedMax = 2;                 // maximum fish speed (pixels / frmae)

// fish frame change
int fishFrameCounter = 1;                   // determines what frame to display        
int fishdeltaTime;                          // time between frame change
const int fishdeltaTimeSlow = 4;            // time between frame change when fish is still
const int fishdeltaTimeMedium = 2;          // time between frame change when fish is at min speed
const int fishdeltaTimeFast = 1;            // time between frame change when fish is at max speed

// fish slow down
int fishSlowDownTimer = 200;
const int fishSlowDownDur = 200;
const float fishSlowDownAmount = 0.5;

// fish computer movement
int fishActionTimer = 500;                  // timer for fish to stop, go min speed, or go max speed
const int fishActionDurMin = 600;           // lower bound for action timer reset
const int fishActionDurMax = 1400;          // upper bound for timer reset
const float fishActionMaxSpeedMult = 0.5;   // multiplier for action duration: max speed
const int fishActionStopMult = 1.5;         // multiplier for action duration: stop
const int fishActionStopChance = 8;         // chance of stopping (1 in 8)
int fishRotationTimer = 100;                // timer for fish to rotate
const int fishRotationWaitMin = 100;        // lower bound for rotate timer reset
const int fishRotationWaitMax = 600;        // upper bound for rotate timer reset

// fish chasing worm mechanics
bool fishLockedOn = false;                  // true if fish needs to lock onto worm
int fishLockOnDesiredRotation = 0;          // final rotation that fish should have
int fishLockOnTimer = 200;                  // time to wait before next lock on action
const int fishLockOnDurMin = 100;           // lower bound for lock on timer
const int fishLockOnDurMax = 450;           // upper bound for lock on timer
const int fishRotationLockOnMult = 0.65;    // multiplier for time between each 45-degree rotation while locking on
const int fishLockOnCloseMult = 0;          // time between lock on actions when fish is close to worm

// worm
const char worm = 'W';                      // how LCDMap identifies worm
int wormRotationTimer = 10;                 // timer for worm to rotate
const int wormRotationDurMin = 100;         // lower bound for rotate timer
const int wormRotationDurMax = 300;         // upper bound for rotate timer
const int wormSpeed = 2;                    // speed of worm (pixels / frame)

// ================================ SETUP & LOOP ===================================

void setup() {
  createFish(fish);

  for (int i = 0; i < cellsCount; ++i) {
    cells[i] = false;
  }

  // map pins to joystick
  pinMode(xPin, INPUT);
  pinMode(yPin, INPUT);
  pinMode(buttonPin, INPUT_PULLUP);

  // generate random seed from open pin
  randomSeed(analogRead(seedPin));

  // initialize LCD display
  lcd.begin(2, 16);
}

void loop() {
  bool joystickUsed = readJoystick();
  switchControl(joystickUsed);

  spawnWorm(worm);
  moveWorm(worm);
  bounceOffBounds(worm, true);

  // fish controlled by player
  if (playerControlled) {
    fishLockedOn = false;
    fishMoving = joystickUsed;
    rotateFishPlayer(fish);
    updateFishPlayer(fish);
    slowDownFish(fish);
  } 

  // fish controlled by computer, chasing worm
  else if (control.contains(worm)) {
    rotateFishComputerChase(fish, worm);
    fishSpeed = fishSpeedMax;
  } 

  // fish controlled by computer, not chasing worm
  else {
    fishLockedOn = false;
    rotateFishComputer(fish);
    updateFishComputer(fish);
    bounceOffBounds(fish, false);
    slowDownFish(fish);
  }

  // if fish mouth overlaps with worm, remove worm
  getEaten(worm, fishGetMouthX(fish), fishGetMouthY(fish));

  // update position and frame
  control.shiftSpriteForward(fish, fishSpeed);
  changeFrameFish(fish);

  // read and draw all custom characters
  draw();

  // delay by deltaTime (plus program loop runtime)
  delay(deltaTime);
}

// ========================= CUSTOM CHARACTER GENERATION ===========================

// --------------------------------- description -----------------------------------
// reads all custom characters and draws them on the lcd
// ---------------------------------------------------------------------------------
void draw() {
  // can only go up to a maximum of 8
  short customCharCount = 0;

  for (int i = 0; i < cellsCount; ++i) {
    short row = (short) i / 16;
    short col = (short) i % 16;

    // get the bytes for a custom character at the current cell
    unsigned char* result = control.readCharacter(row, col);

    // erase current cell if a character will not be drawn again
    if (result == nullptr || customCharCount >= maxCustomChars) {
      if (cells[i]) {
        lcd.setCursor(col, row);
        lcd.print(" ");
        cells[i] = false;
      }
    }

    // if there are still custom characters available, create another one
    if (result != nullptr) {
      lcd.createChar(customCharCount, result);
      lcd.setCursor(col, row);
      lcd.write(byte(customCharCount));
      customCharCount++;
      cells[i] = true;
    }

    delete[] result;
  }
}

// ============================== PLAYER CONTROLLED ================================

// --------------------------------- description -----------------------------------
// rotates fish in direction of joystick
// ---------------------------------------------------------------------------------
void rotateFishPlayer(char id) {
  if (!fishMoving) return;

  // change rotation
  if (joystickX < minThreshL) {
    if (joystickY > minThreshH) control.setSpriteRot(id, 135);
    else if (joystickY < minThreshL) control.setSpriteRot(id, 45);
    else control.setSpriteRot(id, 90);
  }
  else if (joystickX > minThreshH) {
    if (joystickY > minThreshH) control.setSpriteRot(id, 225);
    else if (joystickY < minThreshL) control.setSpriteRot(id, 315);
    else control.setSpriteRot(id, 270);
  }
  else {
    if (joystickY > minThreshH) control.setSpriteRot(id, 180);
    else if (joystickY < minThreshL) control.setSpriteRot(id, 0);
  }
}

// --------------------------------- description -----------------------------------
// moves fish in current direction
// ---------------------------------------------------------------------------------
void updateFishPlayer(char id) {
  if (!fishMoving) return;

  fishSpeed = fishSpeedMin;
  if (joystickX <= maxThreshL || joystickX >= maxThreshH || joystickY <= maxThreshL || joystickY >= maxThreshH) {
    fishSpeed = fishSpeedMax;
  }
}

// =============================== COMPUTER CONTROL ================================

// --------------------------------- description -----------------------------------
// defines a movement behavior for the fish as follows:
// every time the action timer runs out, fish either stops or changes speed
// ---------------------------------------------------------------------------------
void updateFishComputer(char id) {
  if (fishActionTimer <= 0) {
    int actionType = random(0, fishActionStopChance);

    // stop
    if (actionType == 0) {
      fishMoving = false;
      fishActionTimer = random((int) (fishActionDurMin * fishActionStopMult), (fishActionDurMax * fishActionStopMult));
    }

    // if max speed or stopped, change to min speed
    else if (!fishMoving || fishSpeed == fishSpeedMax) {
      fishMoving = true;
      fishSpeed = fishSpeedMin;
      fishActionTimer = random(fishActionDurMin, fishActionDurMax);
    }

    // if min speed, change to max speed
    else if (fishSpeed == fishSpeedMin) {
      fishSpeed = fishSpeedMax;
      fishActionTimer = random((int) (fishActionDurMin * fishActionMaxSpeedMult), (int) (fishActionDurMax * fishActionMaxSpeedMult));
    }
  } else {
    fishActionTimer -= deltaTime;
  }
}

// --------------------------------- description -----------------------------------
// rotates the fish in a random direction whenever the rotation timer runs out
// ---------------------------------------------------------------------------------
void rotateFishComputer(char id) {
  if (!fishMoving) return;

  if (fishRotationTimer <= 0) {
    control.rotateSprite(id, random(-1, 2) * 45);
    fishRotationTimer = random(fishRotationWaitMin, fishRotationWaitMax);
  } else {
    fishRotationTimer -= deltaTime;
  }
}

// --------------------------------- description -----------------------------------
// defines a chasing behavior for the fish as follows:
// 1) fish swims in one direction until the lock on wait timer runs out *
// 2) fish determines final rotation direction towards worm
// 3) fish rotates towards worm in incremenets of 45 until final rotation is reached
// 4) repeat

// * if fish is close enough to worm, wait timer is set to 0, so it rotates rapidly
// ---------------------------------------------------------------------------------
void rotateFishComputerChase(char fishId, char wormId) {
  // if distance is within range, switch to close range lock on multiplier
  if (fishLockOnTimer > fishLockOnDurMax * fishLockOnCloseMult && inRangeFishWorm(fishId, wormId, 15)) {
    fishLockOnTimer = random((int) (fishLockOnDurMin * fishLockOnCloseMult), (int) (fishLockOnDurMax * fishLockOnCloseMult));
  }

  if (!fishLockedOn) {
    fishLockedOn = lockOnFishCruise(fishId, wormId);
  }

  // fish may become locked on during cruising
  if (fishLockedOn) {
    fishLockedOn = lockOnFishAction(fishId, wormId);
  }
}

// --------------------------------- description -----------------------------------
// returns true if fish is locked on, false otherwise
// if rotation timer runs out and fish is not locked on, fish rotates 45 degrees towards
// final lock on direction
// otherwise, if fish is locked on, reset fish cruising timer
// ---------------------------------------------------------------------------------
bool lockOnFishAction(char fishId, char wormId) {  
  if (fishRotationTimer <= 0) {
    int curRotation = control.getSpriteRot(fishId);
    control.rotateSprite(fishId, getLockOnDirection(curRotation, fishLockOnDesiredRotation) * 45);

    // if successfully locked on, switch to cruise mode
    if (curRotation == fishLockOnDesiredRotation) {
      // if fish is close to the worm, decrease cruise mode duration
      if (inRangeFishWorm(fishId, wormId, 15)) {
        fishLockOnTimer = random((int) (fishLockOnDurMin * fishLockOnCloseMult), (int) (fishLockOnDurMax * fishLockOnCloseMult));
      }
      else {
        fishLockOnTimer = random(fishLockOnDurMin, fishLockOnDurMax);
      }

      return false;
    }

    // otherwise, keep trying to lock on
    else {
      fishRotationTimer = random((int) (fishRotationWaitMin * fishRotationLockOnMult), (int) (fishRotationWaitMax * fishRotationLockOnMult));
    }
  }
  
  fishRotationTimer -= deltaTime;
  return true;
}

// --------------------------------- description -----------------------------------
// returns true if fish is locked on, false otherwise
// if fish is done cruising, determines final lock on direction and sets rotation timer
// ---------------------------------------------------------------------------------
bool lockOnFishCruise(char fishId, char wormId) {
  if (fishLockOnTimer <= 0) {
    fishRotationTimer = random((int) (fishRotationWaitMin * fishRotationLockOnMult), (int) (fishRotationWaitMax * fishRotationLockOnMult));

    // determine desired rotation angle
    int curRotation = control.getSpriteRot(fishId);
    int newRotation = getLockOnRotation(fishId, wormId);
    fishLockOnDesiredRotation = (newRotation == curRotation) ? curRotation : newRotation;

    return true;
  }

  fishLockOnTimer -= deltaTime;
  return false;
}

// --------------------------------- description -----------------------------------
// returns the direction that the worm is in, relative to the fish
// based on location of fish mouth, not center of fish
// returns current rotation of the fish if mouth is already overlapping with worm
// returns horizontal direction if fish is far enough from worm (as opposed to diagonal)
// ---------------------------------------------------------------------------------
int getLockOnRotation(char fishId, char wormId) {
  int mouthX = fishGetMouthX(fishId);
  int mouthY = fishGetMouthY(fishId);
  int wormX = control.getSpriteX(wormId);
  int wormY = control.getSpriteY(wormId);
  int wormSize = control.getSpriteSize(wormId);

  // x < targetX, y <>= targetY
  if (mouthX < wormX - 1) {
    if (!inRangeFishWorm(fishId, wormId, 45)) return 90;
    return (mouthY < wormY - 1) ? 135 : ((mouthY > wormY + wormSize) ? 45 : 90);
  }

  // x > targetX, y <>= targetY
  if (mouthX > wormX + wormSize) {
    if (!inRangeFishWorm(fishId, wormId, 45)) return 270;
    return (mouthY < wormY - 1) ? 225 : ((mouthY > wormY + wormSize) ? 315 : 270);
  }

  // x = targetX, y <> targetY
  if (mouthY < wormY - 1) return 180;
  if (mouthY > wormY + wormSize) return 0;

  // x = targetX, y = targetY
  return control.getSpriteRot(fishId);
}

// --------------------------------- description -----------------------------------
// returns the direction in which the fish must rotate
// -1 for counter-clockwise, 0 for none, and 1 for clockwise
// ---------------------------------------------------------------------------------
int getLockOnDirection(int curRotation, int newRotation) {
  if (curRotation == newRotation) return 0;

  for (int i = 1; i <= 3; ++i) {
    if ((curRotation + 45 * i) % 360 == newRotation) return 1;
  }

  return -1;
}

// ==================================== WORM =======================================

// --------------------------------- description -----------------------------------
// spawns worm in a random position if joystick button is clicked and worm is not
// already spawned
// ---------------------------------------------------------------------------------
void spawnWorm(char id) {
  if (joystickButton == 0 && !control.contains(id)) {
    createWorm(id);
    control.shiftSprite(id, random(0, 79), random(0, 15));
  }
}

// --------------------------------- description -----------------------------------
// rotates worm in random direction and moves it forward
// ---------------------------------------------------------------------------------
void moveWorm(char id) {
  if (!control.contains(id)) {
    return;
  }

  // rotation
  if (wormRotationTimer > 0) {
    wormRotationTimer -= (wormRotationTimer - deltaTime < 0) ? wormRotationTimer : deltaTime;
    if (wormRotationTimer == 0) {
      bool dir = random(0, 2);
      control.rotateSprite(id, ((dir) ? 1 : -1) * 45);
      wormRotationTimer = random(wormRotationDurMin, wormRotationDurMax);
    }
  }

  control.shiftSpriteForward(id, wormSpeed);
}

// ================================ MISCELLANEOUS ==================================

// --------------------------------- description -----------------------------------
// updates joystick directional readings, as well as button
// ---------------------------------------------------------------------------------
bool readJoystick() {
  joystickX = analogRead(xPin);
  joystickY = analogRead(yPin);
  joystickButton = digitalRead(buttonPin);

  return (joystickX <= minThreshL || joystickX >= minThreshH || joystickY <= minThreshL || joystickY >= minThreshH);
}

// --------------------------------- description -----------------------------------
// switches fish control from player to AI after a some time of innactivity
// ---------------------------------------------------------------------------------
void switchControl(bool action) {
  if (action) {
    playerControlled = true;
    switchTimer = switchWait;
  } 
  else if (playerControlled) {
    switchTimer -= deltaTime;
    if (switchTimer <= 0) {
      playerControlled = false;
    }
  }
}

// --------------------------------- description -----------------------------------
// removes given sprite if it intersects with the given coordinate
// a one pixel border around the sprite also counts as intersection
// ---------------------------------------------------------------------------------
void getEaten(char id, int x, int y) {
  if (!control.contains(id)) return;

  int sprX = control.getSpriteX(id);
  int sprY = control.getSpriteY(id);
  int siz = control.getSpriteSize(id);

  if (x >= sprX - 1 && x <= sprX + siz && y >= sprY - 1 && y <= sprY + siz) {
    control.removeSprite(id);
  }
}

// --------------------------------- description -----------------------------------
// causes the sprite to bounce off the boundaries if they are set
// if randomize is true, the sprite turns an additional 45 degrees in a random direction
// ---------------------------------------------------------------------------------
void bounceOffBounds(char id, bool randomize) {
  if (!control.atBounds(id)) return;
  if (control.atTopBounds(id) || control.atBotBounds(id)) {
    control.flipSpriteVer(id);
  }
  if (control.atRigBounds(id) || control.atLefBounds(id)) {
    control.flipSpriteHor(id);
  }
  if (randomize) {
    control.rotateSprite(id, random(-1, 2) * 45);
  }
}

// --------------------------------- description -----------------------------------
// returns true if fish and worm are within maxDist of eachother, false otherwise
// ---------------------------------------------------------------------------------
bool inRangeFishWorm(char fishId, char wormId, int maxDist) {
  int mouthX = fishGetMouthX(fishId);
  int mouthY = fishGetMouthY(fishId);
  int wormX = control.getSpriteX(wormId);
  int wormY = control.getSpriteY(wormId);
  return sqrt(pow(wormX - mouthX, 2) + pow(wormY - mouthY, 2)) <= maxDist;
}

// --------------------------------- description -----------------------------------
// updates the fish animation frame based on its current speed
// ---------------------------------------------------------------------------------
void changeFrameFish(char id) {
  if (fishSpeed == fishSpeedMax) {
    fishdeltaTime = fishdeltaTimeFast;
  } 
  else if (fishSpeed == fishSpeedMin) {
    fishdeltaTime = fishdeltaTimeMedium;
  } 
  else {
    fishdeltaTime = fishdeltaTimeSlow;
  }

  // frame only changes when counter reaches current animation frequency
  if (fishFrameCounter >= fishdeltaTime) {
    fishFrameCounter = 1;
    control.nextFrame(id);
  } 
  else {
    fishFrameCounter++;
  }
}

// --------------------------------- description -----------------------------------
// brings the fish to a gradual stop if no movement is applied
// ---------------------------------------------------------------------------------
void slowDownFish(char id) {
  // timer gets reset every time the fish moves
  if (fishMoving) {
    fishSlowDownTimer = fishSlowDownDur;
  }

  // gradually reduce speed until fish speed is 0
  else if (fishSpeed > 0) {
    fishSlowDownTimer -= deltaTime;
    if (fishSlowDownTimer <= 0) {
      fishSpeed -= (fishSpeed - fishSlowDownAmount < 0) ? fishSpeed : fishSlowDownAmount;
      fishSlowDownTimer = fishSlowDownDur;
    }
  }
}

// --------------------------------- description -----------------------------------
// returns X position of the fish mouth based on its current position and rotation
// ---------------------------------------------------------------------------------
int fishGetMouthX(char id) {
  int x = control.getSpriteX(id);
  switch (control.getSpriteRot(id)) {
    case 0: return x + 5;
    case 45: return x + 8;
    case 90: return x + 9;
    case 135: return x + 8;
    case 180: return x + 5;
    case 225: return x + 2;
    case 270: return x + 1;
    case 315: return x + 2;
  }

  return ERR;
}

// --------------------------------- description -----------------------------------
// returns Y position of the fish mouth based on its current position and rotation
// ---------------------------------------------------------------------------------
int fishGetMouthY(char id) {
  int y = control.getSpriteY(id);
  switch (control.getSpriteRot(id)) {
    case 0: return y + 1;
    case 45: return y + 2;
    case 90: return y + 5;
    case 135: return y + 8;
    case 180: return y + 9;
    case 225: return y + 8;
    case 270: return y + 5;
    case 315: return y + 2;
  }

  return ERR;
}

// ================================ INITIALIZATION =================================

// --------------------------------- description -----------------------------------
// initializes worm animation frames and boundaries
// ---------------------------------------------------------------------------------
void createWorm(char id) {
  control.createSprite(id, 2);
  control.addFrame(id, 'A');
  control.drawFrameH(id, 'A', 0, 0);
  control.drawFrameH(id, 'A', 0, 1);
  control.drawFrameD(id, 'A', 0, 1);
  control.drawFrameD(id, 'A', 1, 0);

  control.setBounds(id, 1, 75, 8, 1);
}

// --------------------------------- description -----------------------------------
// initializes fish animation frames and boundaries
// ---------------------------------------------------------------------------------
void createFish(char id) {
  control.createSprite(id, 11);
  addFrameFish1(id);
  addFrameFish2(id);
  addFrameFish3(id);
  addFrameFish4(id);
  addFrameFish5(id);
  addFrameFish6(id);
  addFrameFish7(id);
  addFrameFish8(id);

  control.setBounds(id, 10, 85, 18, 10);
}

void addFrameFish1(char id) {
  char frameId = 'A';

  control.addFrame(id, frameId);
  control.drawFrameH(id, frameId, 5, 0);
  control.drawFrameH(id, frameId, 4, 1);
  control.drawFrameH(id, frameId, 5, 1);
  control.drawFrameH(id, frameId, 6, 1);
  control.drawFrameH(id, frameId, 4, 2);
  control.drawFrameH(id, frameId, 5, 2);
  control.drawFrameH(id, frameId, 6, 2);
  control.drawFrameH(id, frameId, 3, 3);
  control.drawFrameH(id, frameId, 4, 3);
  control.drawFrameH(id, frameId, 5, 3);
  control.drawFrameH(id, frameId, 6, 3);
  control.drawFrameH(id, frameId, 7, 3);
  control.drawFrameH(id, frameId, 4, 4);
  control.drawFrameH(id, frameId, 5, 4);
  control.drawFrameH(id, frameId, 6, 4);
  control.drawFrameH(id, frameId, 4, 5);
  control.drawFrameH(id, frameId, 5, 5);
  control.drawFrameH(id, frameId, 6, 5);
  control.drawFrameH(id, frameId, 5, 6);
  control.drawFrameH(id, frameId, 6, 6);
  control.drawFrameH(id, frameId, 7, 6);
  control.drawFrameH(id, frameId, 6, 7);
  control.drawFrameH(id, frameId, 7, 7);
  control.drawFrameH(id, frameId, 7, 8);
  control.drawFrameH(id, frameId, 6, 9);
  control.drawFrameH(id, frameId, 7, 9);
  control.drawFrameH(id, frameId, 6, 10);

  control.drawFrameD(id, frameId, 8, 1);
  control.drawFrameD(id, frameId, 9, 1);
  control.drawFrameD(id, frameId, 6, 2);
  control.drawFrameD(id, frameId, 7, 2);
  control.drawFrameD(id, frameId, 8, 2);
  control.drawFrameD(id, frameId, 9, 2);
  control.drawFrameD(id, frameId, 6, 3);
  control.drawFrameD(id, frameId, 7, 3);
  control.drawFrameD(id, frameId, 8, 3);
  control.drawFrameD(id, frameId, 5, 4);
  control.drawFrameD(id, frameId, 6, 4);
  control.drawFrameD(id, frameId, 7, 4);
  control.drawFrameD(id, frameId, 8, 4);
  control.drawFrameD(id, frameId, 5, 5);
  control.drawFrameD(id, frameId, 6, 5);
  control.drawFrameD(id, frameId, 4, 6);
  control.drawFrameD(id, frameId, 5, 6);
  control.drawFrameD(id, frameId, 4, 7);
  control.drawFrameD(id, frameId, 5, 7);
  control.drawFrameD(id, frameId, 3, 8);
  control.drawFrameD(id, frameId, 4, 8);
  control.drawFrameD(id, frameId, 1, 9);
  control.drawFrameD(id, frameId, 2, 9);
  control.drawFrameD(id, frameId, 3, 9);
}

void addFrameFish2(char id) {
  char frameId = 'B';

  control.addFrame(id, frameId);
  control.drawFrameH(id, frameId, 5, 0);
  control.drawFrameH(id, frameId, 4, 1);
  control.drawFrameH(id, frameId, 5, 1);
  control.drawFrameH(id, frameId, 6, 1);
  control.drawFrameH(id, frameId, 3, 2);
  control.drawFrameH(id, frameId, 4, 2);
  control.drawFrameH(id, frameId, 5, 2);
  control.drawFrameH(id, frameId, 6, 2);
  control.drawFrameH(id, frameId, 7, 2);
  control.drawFrameH(id, frameId, 2, 3);
  control.drawFrameH(id, frameId, 3, 3);
  control.drawFrameH(id, frameId, 4, 3);
  control.drawFrameH(id, frameId, 5, 3);
  control.drawFrameH(id, frameId, 6, 3);
  control.drawFrameH(id, frameId, 7, 3);
  control.drawFrameH(id, frameId, 8, 3);
  control.drawFrameH(id, frameId, 4, 4);
  control.drawFrameH(id, frameId, 5, 4);
  control.drawFrameH(id, frameId, 6, 4);
  control.drawFrameH(id, frameId, 4, 5);
  control.drawFrameH(id, frameId, 5, 5);
  control.drawFrameH(id, frameId, 6, 5);
  control.drawFrameH(id, frameId, 5, 6);
  control.drawFrameH(id, frameId, 6, 6);
  control.drawFrameH(id, frameId, 5, 7);
  control.drawFrameH(id, frameId, 6, 7);
  control.drawFrameH(id, frameId, 7, 7);
  control.drawFrameH(id, frameId, 6, 8);
  control.drawFrameH(id, frameId, 6, 9);
  control.drawFrameH(id, frameId, 7, 9);
  control.drawFrameH(id, frameId, 7, 10);
  control.drawFrameH(id, frameId, 8, 10);

  control.drawFrameD(id, frameId, 8, 1);
  control.drawFrameD(id, frameId, 9, 1);
  control.drawFrameD(id, frameId, 5, 2);
  control.drawFrameD(id, frameId, 6, 2);
  control.drawFrameD(id, frameId, 7, 2);
  control.drawFrameD(id, frameId, 8, 2);
  control.drawFrameD(id, frameId, 9, 2);
  control.drawFrameD(id, frameId, 6, 3);
  control.drawFrameD(id, frameId, 7, 3);
  control.drawFrameD(id, frameId, 8, 3);
  control.drawFrameD(id, frameId, 5, 4);
  control.drawFrameD(id, frameId, 6, 4);
  control.drawFrameD(id, frameId, 7, 4);
  control.drawFrameD(id, frameId, 8, 4);
  control.drawFrameD(id, frameId, 5, 5);
  control.drawFrameD(id, frameId, 6, 5);
  control.drawFrameD(id, frameId, 8, 5);
  control.drawFrameD(id, frameId, 4, 6);
  control.drawFrameD(id, frameId, 5, 6);
  control.drawFrameD(id, frameId, 4, 7);
  control.drawFrameD(id, frameId, 5, 7);
  control.drawFrameD(id, frameId, 4, 8);
  control.drawFrameD(id, frameId, 4, 9);
  control.drawFrameD(id, frameId, 4, 10);
  control.drawFrameD(id, frameId, 5, 10);
}

void addFrameFish3(char id) {
  char frameId = 'C';

  control.addFrame(id, frameId);
  control.drawFrameH(id, frameId, 5, 0);
  control.drawFrameH(id, frameId, 4, 1);
  control.drawFrameH(id, frameId, 5, 1);
  control.drawFrameH(id, frameId, 6, 1);
  control.drawFrameH(id, frameId, 4, 2);
  control.drawFrameH(id, frameId, 5, 2);
  control.drawFrameH(id, frameId, 6, 2);
  control.drawFrameH(id, frameId, 3, 3);
  control.drawFrameH(id, frameId, 4, 3);
  control.drawFrameH(id, frameId, 5, 3);
  control.drawFrameH(id, frameId, 6, 3);
  control.drawFrameH(id, frameId, 7, 3);
  control.drawFrameH(id, frameId, 2, 4);
  control.drawFrameH(id, frameId, 3, 4);
  control.drawFrameH(id, frameId, 4, 4);
  control.drawFrameH(id, frameId, 5, 4);
  control.drawFrameH(id, frameId, 6, 4);
  control.drawFrameH(id, frameId, 7, 4);
  control.drawFrameH(id, frameId, 8, 4);
  control.drawFrameH(id, frameId, 4, 5);
  control.drawFrameH(id, frameId, 5, 5);
  control.drawFrameH(id, frameId, 6, 5);
  control.drawFrameH(id, frameId, 4, 6);
  control.drawFrameH(id, frameId, 5, 6);
  control.drawFrameH(id, frameId, 4, 7);
  control.drawFrameH(id, frameId, 5, 7);
  control.drawFrameH(id, frameId, 6, 7);
  control.drawFrameH(id, frameId, 5, 8);
  control.drawFrameH(id, frameId, 5, 9);
  control.drawFrameH(id, frameId, 6, 9);
  control.drawFrameH(id, frameId, 6, 10);
  control.drawFrameH(id, frameId, 7, 10);

  control.drawFrameD(id, frameId, 8, 1);
  control.drawFrameD(id, frameId, 9, 1);
  control.drawFrameD(id, frameId, 7, 2);
  control.drawFrameD(id, frameId, 8, 2);
  control.drawFrameD(id, frameId, 9, 2);
  control.drawFrameD(id, frameId, 4, 3);
  control.drawFrameD(id, frameId, 5, 3);
  control.drawFrameD(id, frameId, 6, 3);
  control.drawFrameD(id, frameId, 7, 3);
  control.drawFrameD(id, frameId, 8, 3);
  control.drawFrameD(id, frameId, 5, 4);
  control.drawFrameD(id, frameId, 6, 4);
  control.drawFrameD(id, frameId, 7, 4);
  control.drawFrameD(id, frameId, 4, 5);
  control.drawFrameD(id, frameId, 5, 5);
  control.drawFrameD(id, frameId, 6, 5);
  control.drawFrameD(id, frameId, 7, 5);
  control.drawFrameD(id, frameId, 3, 6);
  control.drawFrameD(id, frameId, 4, 6);
  control.drawFrameD(id, frameId, 5, 6);
  control.drawFrameD(id, frameId, 7, 6);
  control.drawFrameD(id, frameId, 3, 7);
  control.drawFrameD(id, frameId, 4, 7);
  control.drawFrameD(id, frameId, 5, 7);
  control.drawFrameD(id, frameId, 2, 8);
  control.drawFrameD(id, frameId, 3, 8);
  control.drawFrameD(id, frameId, 2, 9);
  control.drawFrameD(id, frameId, 2, 10);
  control.drawFrameD(id, frameId, 3, 10);
}

void addFrameFish4(char id) {
  char frameId = 'D';

  control.addFrame(id, frameId);
  control.drawFrameH(id, frameId, 5, 0);
  control.drawFrameH(id, frameId, 4, 1);
  control.drawFrameH(id, frameId, 5, 1);
  control.drawFrameH(id, frameId, 6, 1);
  control.drawFrameH(id, frameId, 4, 2);
  control.drawFrameH(id, frameId, 5, 2);
  control.drawFrameH(id, frameId, 6, 2);
  control.drawFrameH(id, frameId, 4, 3);
  control.drawFrameH(id, frameId, 5, 3);
  control.drawFrameH(id, frameId, 6, 3);
  control.drawFrameH(id, frameId, 3, 4);
  control.drawFrameH(id, frameId, 4, 4);
  control.drawFrameH(id, frameId, 5, 4);
  control.drawFrameH(id, frameId, 6, 4);
  control.drawFrameH(id, frameId, 7, 4);
  control.drawFrameH(id, frameId, 3, 5);
  control.drawFrameH(id, frameId, 4, 5);
  control.drawFrameH(id, frameId, 5, 5);
  control.drawFrameH(id, frameId, 6, 5);
  control.drawFrameH(id, frameId, 7, 5);
  control.drawFrameH(id, frameId, 4, 6);
  control.drawFrameH(id, frameId, 5, 6);
  control.drawFrameH(id, frameId, 4, 7);
  control.drawFrameH(id, frameId, 5, 7);
  control.drawFrameH(id, frameId, 4, 8);
  control.drawFrameH(id, frameId, 4, 9);
  control.drawFrameH(id, frameId, 5, 9);
  control.drawFrameH(id, frameId, 5, 10);
  control.drawFrameH(id, frameId, 6, 10);

  control.drawFrameD(id, frameId, 8, 1);
  control.drawFrameD(id, frameId, 9, 1);
  control.drawFrameD(id, frameId, 7, 2);
  control.drawFrameD(id, frameId, 8, 2);
  control.drawFrameD(id, frameId, 9, 2);
  control.drawFrameD(id, frameId, 5, 3);
  control.drawFrameD(id, frameId, 6, 3);
  control.drawFrameD(id, frameId, 7, 3);
  control.drawFrameD(id, frameId, 8, 3);
  control.drawFrameD(id, frameId, 4, 4);
  control.drawFrameD(id, frameId, 5, 4);
  control.drawFrameD(id, frameId, 6, 4);
  control.drawFrameD(id, frameId, 7, 4);
  control.drawFrameD(id, frameId, 4, 5);
  control.drawFrameD(id, frameId, 5, 5);
  control.drawFrameD(id, frameId, 6, 5);
  control.drawFrameD(id, frameId, 7, 5);
  control.drawFrameD(id, frameId, 2, 6);
  control.drawFrameD(id, frameId, 3, 6);
  control.drawFrameD(id, frameId, 4, 6);
  control.drawFrameD(id, frameId, 5, 6);
  control.drawFrameD(id, frameId, 6, 6);
  control.drawFrameD(id, frameId, 1, 7);
  control.drawFrameD(id, frameId, 2, 7);
  control.drawFrameD(id, frameId, 3, 7);
  control.drawFrameD(id, frameId, 1, 8);
  control.drawFrameD(id, frameId, 1, 9);
  control.drawFrameD(id, frameId, 2, 10);
}

void addFrameFish5(char id) {
  char frameId = 'E';

  control.addFrame(id, frameId);
  control.drawFrameH(id, frameId, 5, 0);
  control.drawFrameH(id, frameId, 4, 1);
  control.drawFrameH(id, frameId, 5, 1);
  control.drawFrameH(id, frameId, 6, 1);
  control.drawFrameH(id, frameId, 4, 2);
  control.drawFrameH(id, frameId, 5, 2);
  control.drawFrameH(id, frameId, 6, 2);
  control.drawFrameH(id, frameId, 3, 3);
  control.drawFrameH(id, frameId, 4, 3);
  control.drawFrameH(id, frameId, 5, 3);
  control.drawFrameH(id, frameId, 6, 3);
  control.drawFrameH(id, frameId, 7, 3);
  control.drawFrameH(id, frameId, 4, 4);
  control.drawFrameH(id, frameId, 5, 4);
  control.drawFrameH(id, frameId, 6, 4);
  control.drawFrameH(id, frameId, 4, 5);
  control.drawFrameH(id, frameId, 5, 5);
  control.drawFrameH(id, frameId, 6, 5);
  control.drawFrameH(id, frameId, 3, 6);
  control.drawFrameH(id, frameId, 4, 6);
  control.drawFrameH(id, frameId, 5, 6);
  control.drawFrameH(id, frameId, 3, 7);
  control.drawFrameH(id, frameId, 4, 7);
  control.drawFrameH(id, frameId, 3, 8);
  control.drawFrameH(id, frameId, 3, 9);
  control.drawFrameH(id, frameId, 4, 9);
  control.drawFrameH(id, frameId, 4, 10);

  control.drawFrameD(id, frameId, 8, 1);
  control.drawFrameD(id, frameId, 9, 1);
  control.drawFrameD(id, frameId, 6, 2);
  control.drawFrameD(id, frameId, 7, 2);
  control.drawFrameD(id, frameId, 8, 2);
  control.drawFrameD(id, frameId, 9, 2);
  control.drawFrameD(id, frameId, 6, 3);
  control.drawFrameD(id, frameId, 7, 3);
  control.drawFrameD(id, frameId, 8, 3);
  control.drawFrameD(id, frameId, 5, 4);
  control.drawFrameD(id, frameId, 6, 4);
  control.drawFrameD(id, frameId, 7, 4);
  control.drawFrameD(id, frameId, 8, 4);
  control.drawFrameD(id, frameId, 3, 5);
  control.drawFrameD(id, frameId, 4, 5);
  control.drawFrameD(id, frameId, 5, 5);
  control.drawFrameD(id, frameId, 6, 5);
  control.drawFrameD(id, frameId, 2, 6);
  control.drawFrameD(id, frameId, 3, 6);
  control.drawFrameD(id, frameId, 4, 6);
  control.drawFrameD(id, frameId, 1, 7);
  control.drawFrameD(id, frameId, 2, 7);
  control.drawFrameD(id, frameId, 1, 8);
  control.drawFrameD(id, frameId, 1, 9);
}

void addFrameFish6(char id) {
  char frameId = 'F';

  control.addFrame(id, frameId);
  control.drawFrameH(id, frameId, 5, 0);
  control.drawFrameH(id, frameId, 4, 1);
  control.drawFrameH(id, frameId, 5, 1);
  control.drawFrameH(id, frameId, 6, 1);
  control.drawFrameH(id, frameId, 4, 2);
  control.drawFrameH(id, frameId, 5, 2);
  control.drawFrameH(id, frameId, 6, 2);
  control.drawFrameH(id, frameId, 3, 3);
  control.drawFrameH(id, frameId, 4, 3);
  control.drawFrameH(id, frameId, 5, 3);
  control.drawFrameH(id, frameId, 6, 3);
  control.drawFrameH(id, frameId, 7, 3);
  control.drawFrameH(id, frameId, 2, 4);
  control.drawFrameH(id, frameId, 3, 4);
  control.drawFrameH(id, frameId, 4, 4);
  control.drawFrameH(id, frameId, 5, 4);
  control.drawFrameH(id, frameId, 6, 4);
  control.drawFrameH(id, frameId, 7, 4);
  control.drawFrameH(id, frameId, 8, 4);
  control.drawFrameH(id, frameId, 4, 5);
  control.drawFrameH(id, frameId, 5, 5);
  control.drawFrameH(id, frameId, 6, 5);
  control.drawFrameH(id, frameId, 4, 6);
  control.drawFrameH(id, frameId, 5, 6);
  control.drawFrameH(id, frameId, 3, 7);
  control.drawFrameH(id, frameId, 4, 7);
  control.drawFrameH(id, frameId, 5, 7);
  control.drawFrameH(id, frameId, 4, 8);
  control.drawFrameH(id, frameId, 3, 9);
  control.drawFrameH(id, frameId, 4, 9);
  control.drawFrameH(id, frameId, 2, 10);
  control.drawFrameH(id, frameId, 3, 10);

  control.drawFrameD(id, frameId, 8, 1);
  control.drawFrameD(id, frameId, 9, 1);
  control.drawFrameD(id, frameId, 7, 2);
  control.drawFrameD(id, frameId, 8, 2);
  control.drawFrameD(id, frameId, 9, 2);
  control.drawFrameD(id, frameId, 4, 3);
  control.drawFrameD(id, frameId, 5, 3);
  control.drawFrameD(id, frameId, 6, 3);
  control.drawFrameD(id, frameId, 7, 3);
  control.drawFrameD(id, frameId, 8, 3);
  control.drawFrameD(id, frameId, 5, 4);
  control.drawFrameD(id, frameId, 6, 4);
  control.drawFrameD(id, frameId, 7, 4);
  control.drawFrameD(id, frameId, 0, 5);
  control.drawFrameD(id, frameId, 3, 5);
  control.drawFrameD(id, frameId, 4, 5);
  control.drawFrameD(id, frameId, 5, 5);
  control.drawFrameD(id, frameId, 6, 5);
  control.drawFrameD(id, frameId, 7, 5);
  control.drawFrameD(id, frameId, 0, 6);
  control.drawFrameD(id, frameId, 1, 6);
  control.drawFrameD(id, frameId, 2, 6);
  control.drawFrameD(id, frameId, 3, 6);
  control.drawFrameD(id, frameId, 4, 6);
  control.drawFrameD(id, frameId, 7, 6);
}

void addFrameFish7(char id) {
  char frameId = 'G';

  control.addFrame(id, frameId);
  control.drawFrameH(id, frameId, 5, 0);
  control.drawFrameH(id, frameId, 4, 1);
  control.drawFrameH(id, frameId, 5, 1);
  control.drawFrameH(id, frameId, 6, 1);
  control.drawFrameH(id, frameId, 4, 2);
  control.drawFrameH(id, frameId, 5, 2);
  control.drawFrameH(id, frameId, 6, 2);
  control.drawFrameH(id, frameId, 4, 3);
  control.drawFrameH(id, frameId, 5, 3);
  control.drawFrameH(id, frameId, 6, 3);
  control.drawFrameH(id, frameId, 3, 4);
  control.drawFrameH(id, frameId, 4, 4);
  control.drawFrameH(id, frameId, 5, 4);
  control.drawFrameH(id, frameId, 6, 4);
  control.drawFrameH(id, frameId, 7, 4);
  control.drawFrameH(id, frameId, 3, 5);
  control.drawFrameH(id, frameId, 4, 5);
  control.drawFrameH(id, frameId, 5, 5);
  control.drawFrameH(id, frameId, 6, 5);
  control.drawFrameH(id, frameId, 7, 5);
  control.drawFrameH(id, frameId, 5, 6);
  control.drawFrameH(id, frameId, 6, 6);
  control.drawFrameH(id, frameId, 4, 7);
  control.drawFrameH(id, frameId, 5, 7);
  control.drawFrameH(id, frameId, 6, 7);
  control.drawFrameH(id, frameId, 5, 8);
  control.drawFrameH(id, frameId, 4, 9);
  control.drawFrameH(id, frameId, 5, 9);
  control.drawFrameH(id, frameId, 3, 10);
  control.drawFrameH(id, frameId, 4, 10);

  control.drawFrameD(id, frameId, 8, 1);
  control.drawFrameD(id, frameId, 9, 1);
  control.drawFrameD(id, frameId, 7, 2);
  control.drawFrameD(id, frameId, 8, 2);
  control.drawFrameD(id, frameId, 9, 2);
  control.drawFrameD(id, frameId, 5, 3);
  control.drawFrameD(id, frameId, 6, 3);
  control.drawFrameD(id, frameId, 7, 3);
  control.drawFrameD(id, frameId, 8, 3);
  control.drawFrameD(id, frameId, 4, 4);
  control.drawFrameD(id, frameId, 5, 4);
  control.drawFrameD(id, frameId, 6, 4);
  control.drawFrameD(id, frameId, 7, 4);
  control.drawFrameD(id, frameId, 3, 5);
  control.drawFrameD(id, frameId, 4, 5);
  control.drawFrameD(id, frameId, 5, 5);
  control.drawFrameD(id, frameId, 6, 5);
  control.drawFrameD(id, frameId, 7, 5);
  control.drawFrameD(id, frameId, 3, 6);
  control.drawFrameD(id, frameId, 4, 6);
  control.drawFrameD(id, frameId, 5, 6);
  control.drawFrameD(id, frameId, 6, 6);
  control.drawFrameD(id, frameId, 0, 7);
  control.drawFrameD(id, frameId, 2, 7);
  control.drawFrameD(id, frameId, 3, 7);
  control.drawFrameD(id, frameId, 4, 7);
  control.drawFrameD(id, frameId, 0, 8);
  control.drawFrameD(id, frameId, 1, 8);
  control.drawFrameD(id, frameId, 2, 8);
}

void addFrameFish8(char id) {
  char frameId = 'H';

  control.addFrame(id, frameId);
  control.drawFrameH(id, frameId, 5, 0);
  control.drawFrameH(id, frameId, 4, 1);
  control.drawFrameH(id, frameId, 5, 1);
  control.drawFrameH(id, frameId, 6, 1);
  control.drawFrameH(id, frameId, 4, 2);
  control.drawFrameH(id, frameId, 5, 2);
  control.drawFrameH(id, frameId, 6, 2);
  control.drawFrameH(id, frameId, 4, 3);
  control.drawFrameH(id, frameId, 5, 3);
  control.drawFrameH(id, frameId, 6, 3);
  control.drawFrameH(id, frameId, 4, 4);
  control.drawFrameH(id, frameId, 5, 4);
  control.drawFrameH(id, frameId, 6, 4);
  control.drawFrameH(id, frameId, 4, 5);
  control.drawFrameH(id, frameId, 5, 5);
  control.drawFrameH(id, frameId, 6, 5);
  control.drawFrameH(id, frameId, 5, 6);
  control.drawFrameH(id, frameId, 6, 6);
  control.drawFrameH(id, frameId, 5, 7);
  control.drawFrameH(id, frameId, 6, 7);
  control.drawFrameH(id, frameId, 6, 8);
  control.drawFrameH(id, frameId, 5, 9);
  control.drawFrameH(id, frameId, 6, 9);
  control.drawFrameH(id, frameId, 4, 10);
  control.drawFrameH(id, frameId, 5, 10);

  control.drawFrameD(id, frameId, 8, 1);
  control.drawFrameD(id, frameId, 9, 1);
  control.drawFrameD(id, frameId, 6, 2);
  control.drawFrameD(id, frameId, 7, 2);
  control.drawFrameD(id, frameId, 8, 2);
  control.drawFrameD(id, frameId, 9, 2);
  control.drawFrameD(id, frameId, 5, 3);
  control.drawFrameD(id, frameId, 6, 3);
  control.drawFrameD(id, frameId, 7, 3);
  control.drawFrameD(id, frameId, 8, 3);
  control.drawFrameD(id, frameId, 5, 4);
  control.drawFrameD(id, frameId, 6, 4);
  control.drawFrameD(id, frameId, 7, 4);
  control.drawFrameD(id, frameId, 4, 5);
  control.drawFrameD(id, frameId, 5, 5);
  control.drawFrameD(id, frameId, 6, 5);
  control.drawFrameD(id, frameId, 4, 6);
  control.drawFrameD(id, frameId, 5, 6);
  control.drawFrameD(id, frameId, 3, 7);
  control.drawFrameD(id, frameId, 4, 7);
  control.drawFrameD(id, frameId, 0, 8);
  control.drawFrameD(id, frameId, 3, 8);
  control.drawFrameD(id, frameId, 4, 8);
  control.drawFrameD(id, frameId, 1, 9);
  control.drawFrameD(id, frameId, 2, 9);
  control.drawFrameD(id, frameId, 3, 9);
}
