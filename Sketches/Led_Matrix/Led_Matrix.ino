// MADE BY NICOLAS TONG
const int l = 4; //Length of the matrix
const int w = 6; //Width of the matrix

const int pinRows[4] = {2, 3, 4, 5}; //Row or length pin numbers
const int pinColumns[6] = {11, 10, 9, 8, 7, 6}; //Column or width pin numbers

//The pins that are on:

bool pinOn[24] = { //Starting logo, an "N" and a "T"
  0, 0, 0, 1, 1, 1,
  1, 1, 1, 0, 1, 0,
  1, 0, 1, 0, 1, 0,
  1, 0, 1, 0, 0, 0
}; // THIS ARRAY IS THE DRIVING FORCE OF THIS PROJECT: NO MESS UP

const int lsize = (sizeof(pinRows) / sizeof(int));
const int wsize = (sizeof(pinColumns) / sizeof(int));

//Asteroid App Variables:
const bool AsteroidScreen[24] = { //currently unused!
  1, 0, 0, 0, 0, 0,
  1, 1, 0, 0, 0, 0,
  1, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0
};

const bool AsteroidSkull[24] = { //shown at death
  0, 1, 1, 1, 1, 0,
  0, 1, 0, 0, 1, 0,
  0, 0, 1, 1, 0, 0,
  0, 1, 0, 0, 1, 0
};

bool shipLeft = false;
bool shipRight = false;

bool astDead = false; //Is the player dead?
int astKiller = 0; //The asteroid that killed the player
int shipShift = 0;
const byte shipLoc[4] = {13, 18, 19, 20}; //The ship's four dots
bool astLoc[6] = {0, 0, 0, 0, 0, 0}; //The asteroid stream
int level = 1; //The level of the asteroid game

//Timer vars:
unsigned int ast_time = 600; //The rate at which the screen "shifts" down
unsigned int ast_lastMillis = 0;
const int astSk_time = 2000; //For the skull
unsigned int astSk_lastMillis = 0;
unsigned int astNum = 0; //The number for counting how many asteroids every 3 lines

const int numOfApps = 5; //basically the size of the menu / 24

//The menu screen
const bool menu[120] = {
  0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0,
  0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 0, 0, 1, 0,
  0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 0, 0, 1, 0,
  0, 0, 0, 1, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 1, 0,
};

const int mlen = sizeof(menu) / l;
int mnctr = 0;

//If the menu is moving
bool menuMoving = false;

//The menu selection
int menuSel = 1;

//App Bools
bool AppRunning = false;
bool SettingsRunning = false;
bool FlashlightRunning = false;
bool AsteroidRunning = false;
bool ObstacleRunning = false;

//Buttons
const int buttonOne = 13; //Pins
const int buttonTwo = 12;
bool leftPress = false;
bool rightPress = false;
bool bothPress = false;

//Timer vars
const int ntime = 100;
bool finished = true;
unsigned long lastMillis = 0;

int i; //Used in most for loops in this code
// The row (length) pins must supply power!!

void setup() {
  // put your setup code here, to run once:
  pinMode(buttonOne, INPUT);
  pinMode(buttonTwo, INPUT);
  Serial.begin(9600);
  ResetAll();

  //Weird workaround:
  UpdateDisplay();
  delay(30);
  UpdateDisplay();
  delay(30);
  UpdateDisplay();
  delay(30);
  UpdateDisplay();
  delay(30);
  UpdateDisplay();
  delay(30);
  UpdateDisplay();
  delay(30);
  double hello = prettyRandomSeed ();
  ReturnToMenu();
}


void loop() {

  //Updates the button bool status
  UpdateButton();

  //-----Flashlight App-----
  if (menuSel % numOfApps == 4 && rightPress && !AppRunning) {
    AppRunning = true;
    FlashlightAppSetup();
    FlashlightRunning = true;
  }

  if (FlashlightRunning) {
    FlashlightAppLoop();
  }
  //-----Flashlight App-----

  //-----Asteroid App-----
  if (menuSel % numOfApps == 2 && rightPress && !AppRunning) {
    AppRunning = true;
    AsteroidAppSetup();
    AsteroidRunning = true;
  }

  if (AsteroidRunning) {
    AsteroidAppLoop();
  }
  //-----Asteroid App-----

  //Shifting the menu:
  if (millis() - lastMillis > ntime) {
    lastMillis = millis() - 1;
    if (menuMoving) {
      ShiftMenu();
      if ((mnctr % w) == 0) {
        SwitchSelection();
        menuMoving = false;
      }
    }
  }

  //Switching the icon if the left button is pressed
  if (leftPress && !menuMoving && !AppRunning) {
    menuMoving = true;
  }
  RowUpdate();

  //END OF LOOP
}

void ResetAll() {
  for (i = 0; i < lsize; i++) {
    pinMode(pinRows[i], OUTPUT);
    digitalWrite(pinRows[i], 0);

  }
  for (i = 0; i < wsize; i++) {
    pinMode(pinColumns[i], OUTPUT);
    digitalWrite(pinColumns[i], 5);
  }
}

void ResetPinOn() {
  for (i = 0; i < 24; i++) {
    pinOn[i] = 0;
  }
}

void RowUpdate () {
  int k;
  int j;
  for (k = 0; k < w; k++) {
    digitalWrite(pinColumns[k], 0);
    for (j = 0; j < l; j++) {
      if (pinOn[j * w + k] == 1) {
        digitalWrite(pinRows[j], 5);
      }
    }
    delay(1); //Delays so that you can actually see something
    ResetAll();
  }
}

void LEDUpdate (int LED) {
  int mod = (LED % 6);
  digitalWrite(pinRows[mod], 0);
  digitalWrite(pinColumns[(LED - mod) / 6], 5);
}

//Updates display without reset
void UpdateDisplay() {
  int k;
  int j;
  for (k = 0; k < w; k++) {
    digitalWrite(pinColumns[k], 0);
    for (j = 0; j < l; j++) {
      if (pinOn[j * w + k] == 1) {
        digitalWrite(pinRows[j], 5);
      }
    }
    delay(1);
    ResetAll();
  }
}

void ShiftLeft(int one, int two, int three, int four) {
  int k;
  int h;
  for (k = 0; k < l; k++) {
    for (h = 0; h < 5; h++) {
      pinOn[k * 6 + h] = pinOn[k * 6 + h + 1];
    }
    switch (k) {
      case 0:
        pinOn[k * 6 + 5] = one;
        break;
      case 1:
        pinOn[k * 6 + 5] = two;
        break;
      case 2:
        pinOn[k * 6 + 5] = three;
        break;
      case 3:
        pinOn[k * 6 + 5] = four;
        break;
    }
  }
}

void ShiftDown(int one, int two, int three, int four, int five, int six) {
  int k;
  int h;
  for (k = 5; k > -1; k--) {
    for (h = 3; h > -1; h--) {
      pinOn[k + (h * 6)] = pinOn[k + ((h - 1) * 6)];
    }
    switch (k) {
      case 0:
        pinOn[k] = one;
        break;
      case 1:
        pinOn[k] = two;
        break;
      case 2:
        pinOn[k] = three;
        break;
      case 3:
        pinOn[k] = four;
        break;
      case 4:
        pinOn[k] = five;
        break;
      case 5:
        pinOn[k] = six;
        break;

    }

  }
}

void ShiftMenu() {
  ShiftLeft(menu[mnctr % mlen], menu[(mnctr % mlen) + mlen], menu[(mnctr % mlen) + mlen * 2], menu[(mnctr % mlen) + mlen * 3]);
  if (mnctr == mlen) {
    mnctr = 0;
  }
  mnctr++;
}

void SwitchSelection() {
  menuSel++;
}

void UpdateButton() { //Updates the button selections
  if (digitalRead(buttonOne)) {
    leftPress = true;
  }
  else {
    leftPress = false;
  }
  if (digitalRead(buttonTwo)) {
    rightPress = true;
  }
  else {
    rightPress = false;
  }
  if (digitalRead(buttonOne) && digitalRead(buttonTwo)) {
    bothPress = true;
  }
  else {
    bothPress = false;
  }
}
//-----Flashlight App Voids:-----
void FlashlightAppSetup() {
  for (i = 0; i < 6; i++) {
    ShiftLeft(1, 1, 1, 1);
  }
}

void FlashlightAppLoop() {
  if (bothPress) {
    ReturnToMenu();
  }
}

//-----Asteroid Dodge App Voids:-----
void AsteroidAppSetup() {
  shipShift = 0;
  astDead = false;
  ast_lastMillis = 0;
  astSk_lastMillis = 0;
  ast_time = 600;
  astKiller = 0; //The asteroid that killed the player
  ResetPinOn();
  for (i = 0; i < 6; i++) { //Reseting asteriod position so
    astLoc[i] = 0;          //they don't auto kill
  }


  randomSeed(prettyRandomSeed()); //Getting a value from analog pin
  for (i = 0; i < 4; i++) {
    pinOn[shipLoc[i]] = 1;
  }
}

void AsteroidAppLoop() {
  if (!astDead) {
    if (bothPress) {
      ReturnToMenu();
      return;
    }

    if (leftPress) {
      shipLeft = true;
    }

    if (shipLeft) {
      if (!leftPress) {
        if (shipShift < 3) {
          EraseShip();
          shipShift += 1;
          shipLeft = false;
          MoveShip();
        }
      }
    }

    if (rightPress) {
      shipRight = true;
    }

    if (shipRight) {
      if (!rightPress) {
        if (shipShift > 0) {
          EraseShip();
          shipShift -= 1;
          shipRight = false;
          MoveShip();
        }
      }
    }

    //Speed system default is 600 (refer to ast_time)
    if (astNum > 200) {
      ast_time = 350;
    } else if (astNum > 100) {
      ast_time = 400;
    } else if (astNum > 50) {
      ast_time = 450;
    } else if (astNum > 20) {
      ast_time = 500;
    } else if (astNum > 10) {
      ast_time = 550;
    }

    if (millis() - ast_lastMillis > ast_time) {
      ast_lastMillis = millis() - 1;
      astNum++;
      if (astNum % 4 == 0) { //The modulus describes the spacing between the asteroids
        ResetAll();
        GenAsteroid();
        ShiftDown(astLoc[0], astLoc[1], astLoc[2], astLoc[3], astLoc[4], astLoc[5]);
      } else {
        ShiftDown(0, 0, 0, 0, 0, 0);
      }
      CheckColl();
      MoveShip();
    }
  } else {
    for (i = 0; i < 24; i++) {
      pinOn[i] = AsteroidSkull[i];
    }
    RowUpdate();
    if (millis() - astSk_lastMillis > astSk_time) {
      astSk_lastMillis = millis() - 1;
      ReturnToMenu();
    }
  }
}

void CheckColl() {

  if (pinOn[shipLoc[0] + (shipShift)] == 1) {
    astKiller = shipLoc[0] + (shipShift);
    astDead = true;
    astSk_lastMillis = millis();
  }

  if (pinOn[shipLoc[1] + (shipShift)] == 1) {
    astKiller = shipLoc[1] + (shipShift);
    astDead = true;
    astSk_lastMillis = millis();
  }

  //We don't check two because it is in the middle of the ship

  if (pinOn[shipLoc[3] + (shipShift)] == 1) {
    astKiller = shipLoc[3] + (shipShift);
    astDead = true;
    astSk_lastMillis = millis();
  }
}


void MoveShip() {
  //Moving the ship left and right
  for (i = 0; i < 4; i++) {
    if (pinOn[shipLoc[i] + (shipShift)] == 0) {
      pinOn[shipLoc[i] + (shipShift)] = 1;
    }
  }
}

void EraseShip() {
  for (int i = 0; i < 4; i++) {
    pinOn[shipLoc[i] + (shipShift)] = 0;
  }
}

void GenAsteroid () {
  for (i = 0; i < 6; i++) {
    astLoc[i] = 0; // Reset astLoc to all 0
  }
  int randomAst = random(6); // random number for the first asteroid
  astLoc[randomAst] = 1;
  int randomAstTwo = getNewRandNum();
  while (!checkAst(randomAst, randomAstTwo)) {
    randomAstTwo = getNewRandNum();
  }
  astLoc[randomAstTwo] = 1;
}

boolean checkAst(int astOne, int astTwo) {
  if (astOne == astTwo) {
    return false;
  }
  if ((astOne - astTwo) == 3) {
    //FAIL
    //100100, 010010, 001001 (UNPASSIBLE)
    return false;
  }

  if ((astTwo - astOne) == 3) {
    //FAIL
    //100100, 010010, 001001 (UNPASSIBLE)
    return false;
  }

  if (astOne == 2 && astTwo == 3) {
    return false;
    //001100 UNPASSIBLE
  }

  if (astOne == 3 && astTwo == 2) {
    return false;
    //001100 UNPASSIBLE
  }

  if ((astOne == 2 && astTwo == 4) || (astOne == 4 && astTwo == 2)) {
    return false;
    //001010 UNPASSIBLE

  }
  if ((astOne == 3 && astTwo == 1) || (astOne == 1 && astTwo == 3)) {
    return false;
    //010100 UNPASSIBLE
  }
  return true;
}

int getNewRandNum () {
  return random(6); //Random number genterator for asteroid game only
}

double prettyRandomSeed () {
  double randTemp = analogRead(0);
  randTemp = randTemp/100;
  randTemp = randTemp*randTemp;
  randTemp = randTemp*10000;
  randTemp = randTemp/700;
  randTemp = randTemp*randTemp;
  randTemp = randTemp*7;
  Serial.println(randTemp);
  return 0;
}

void ReturnToMenu() {
  menuSel = 1;
  mnctr = 0;
  leftPress = false; //Reseting button presses (have a feeling its causing issues...)
  rightPress = false;
  bothPress = false;

  AppRunning = false; //How apps were made in the old days LOL!
  SettingsRunning = false;
  FlashlightRunning = false;
  AsteroidRunning = false;
  ObstacleRunning = false;
  ResetAll();
  ResetPinOn();
  for (i = 0; i < 6; i++) {
    ShiftMenu();
    delay(100);
  }
}

