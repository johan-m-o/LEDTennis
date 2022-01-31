/*
   LED Tennis v1.1
   Copyright (c) 2022 Johan Oscarsson
   Released under the MIT licence

   Documentation and project comments can be found on Github:
   https://www.github.com/johan-m-o/LEDTennis
*/

#include <FastLED.h>              // FastLED LED control library https://github.com/FastLED/FastLED
#include <SSD1306Ascii.h>         // Minimal SSD1306 OLED control library (only text) https://github.com/greiman/SSD1306Ascii Using this rather than Adafruit's SSD1306 library saves about 8 kB of program storage and 300 bytes of dynamic memory!
#include <SSD1306AsciiAvrI2c.h>   // Small and fast communications class for the SSD1306 ASCII library

/*****************
 *   Variables   *
 *****************/

/* OLED display */
#define SCREEN_WIDTH 128
#define OLED_RESET 4
#define SCREEN_ADDRESS 0x3C
SSD1306AsciiAvrI2c display;
byte scoreP1x, scoreP2x, totalRoundsX, clicksP1x, clicksP2x, scoreY, clicksY, fontW = 8, leftButtonMarker = fontW * 1.5, rightButtonMarker = SCREEN_WIDTH - fontW * 2.5;

/* LEDs */
#define NUM_LEDS 60
CRGB leds[NUM_LEDS];
CRGB ledColour;
byte ledHue, ledBrightness = 74, minBright = 20, maxBright = 200, ledBright, ledFade, ledPos, prevPos, randomLED;
int ledFadeTimer = 3000;
bool ledUp;

/* Buttons */
const byte bPlayer1 = 2, bPlayer2 = 3, bLeft = 6, bRight = 5, bMode = 4;
bool bPlayer1Press = false, bPlayer2Press = false, bPlayer1State = false, bPlayer2State = false, bLeftPress = false, bRightPress = false, bModePress = false;
unsigned long bPlayer1EventMS = 0, bPlayer2EventMS = 0, bChangeEventMS = 0, bModeEventMS = 0;
unsigned long bPlayer1PressDurationMS = 0, bPlayer2PressDurationMS = 0, bChangePressDurationMS = 0, bModePressDurationMS = 0;
byte debounceTime = 50;

/* Settings */
byte difficultyLevels = 5, difficultyP1 = 3, difficultyP2 = 3, playMode = 0, menuItem = 0, menuMax = 4;
char *gameMode[] = {"Tennis", "React", "Tug-O-War", "Minigolf"};
byte numGames = sizeof(gameMode) / sizeof(gameMode[0]);
bool drawMenu = true;

/* Game-play */
byte pickPlayer = 99, prevPlayer, playerCount = 0, p1LEDDelay, p2LEDDelay, difficultyMultiplier, baseDifficulty = 100, reactP1 = HIGH, reactP2 = HIGH;
byte iFrom, iTo, ctrlCount, markPos, chargeTo, chargeCharging, maxTennisSpeed = 5;
int ledDelay, countP1 = 0, countP2 = 0, p1Score = 0, p2Score = 0;
int firstTo = 0, totalRounds = 0, reactPress1 = 0, reactPress2 = 0, reactPress1Tot = 0, reactPress2Tot = 0;
unsigned long ledTimerMS = 0, randomDelayMS = 0, chargeMax = 6000, chargeUpTime = 0, buttonBlockMS = 0;
bool startPlay = false, stopPlay = false, beforeStart = true, pauseYesNo = false, stopBeginLoop = false, rainbowPulse = false;
bool reactPress = false, reactWrong = false, minigolfPlay = false;

/*****************
 *   Functions   *
 *****************/
/* OLED-display */
// Draw the main menu and settings menus
void menuDrawFn(String title, String line1, String line2, String line3) {
  display.clear();
  
  display.setCursor((SCREEN_WIDTH - (title.length() * fontW)) / 2,0);
  display.print(title);
  display.setCursor((SCREEN_WIDTH - (line1.length() * fontW)) / 2,1);
  if (menuItem == 3 && line1 == "First to 0" ) {
    display.print(F("First to "));
    display.write(236); // Infinity sign (but you couldn't guess by the way it looks on the display)
  } else {
    display.print(line1);
  }
  display.setCursor((SCREEN_WIDTH - (line2.length() * fontW)) / 2,2);
  display.print(line2);
  display.setCursor((SCREEN_WIDTH - (line3.length() * fontW)) / 2,3);
  display.print(line3);
  if (menuItem > 0) {
    display.setCursor(leftButtonMarker,3);
    if (menuItem != 2) {
      display.write(17); // Arrow left
    } else {
      if (playMode != 3) {
        display.write(30); // Arrow up
      }
    }
    display.setCursor(rightButtonMarker,3);
    if (menuItem != 2) {
      display.write(16); // Arrow right
    } else {
      display.write(30); // Arrow up
    }
  }
}

/* LEDs */
// Draw the two center LEDs for Tug-O-War
void centerLEDs(byte pos, CRGB colour) {
  leds[pos] = colour;
  leds[pos + 1] = colour;
}

/* Buttons */
// Check if the button press should send the light back
void tennisShot(byte ledFrom, byte ledTo, byte ledStart, bool ledDirection) {
  byte tmpCount, tmpDelay;

  // Calculate if LED should be returned depending on player difficulty level
  if (ledPos >= ledFrom && ledPos < ledTo) {
    ledPos = ledStart;
    ledUp = ledDirection;
    if (pickPlayer == 1) {
      countP1++;
      tmpCount = countP1 * difficultyP1; // Increase the LED speed based on player difficulty
      tmpDelay = p1LEDDelay;
      pickPlayer = 0;
    } else if (pickPlayer == 0) {
      countP2++;
      tmpCount = countP2 * difficultyP2; // Increase the LED speed based on player difficulty
      tmpDelay = p2LEDDelay;
      pickPlayer = 1;
    }

    // See if maximum LED speed has been reached or not
    if (tmpDelay - tmpCount < maxTennisSpeed) {
      ledDelay = maxTennisSpeed;
    } else {
      ledDelay = tmpDelay - tmpCount;
    }

    // Reset blocked button press
    buttonBlockMS = 0;

    // Count the number of consecutive hits
    totalRounds++;
    
  } else {
    // Block button press
    buttonBlockMS = millis();
  }
}

/* Settings */
// Change difficulty setting
byte incrementDiff(byte in) {
  byte out;
  if (in == difficultyLevels) {
    out = 1;
    
  } else {
    in++;
    out = in;
  }
  
  return out;
}

/* Game-play */
// Draw the LEDs to hit in Minigolf
void markPosition(byte mark, byte ball) {
  byte difficulty = difficultyLevels - (difficultyP1 - 1);
  ctrlCount = 0;

  while (ctrlCount < difficulty) {
    if (mark + ctrlCount != ball) {
      leds[mark + ctrlCount] = CRGB::Green;
    }
    ctrlCount++;
  }
  ctrlCount = 0;
}

// End of round calculations to see if points should be awarded or game ended
void gameEnd(bool win) {
  if (!stopPlay) {
    FastLED.clear();
    
    if (playMode == 0 && win) { // Tennis
      if (pickPlayer == 1) {
        p1Score++;
        iFrom = NUM_LEDS / 2;
        iTo = NUM_LEDS;
        pickPlayer = 0;
      } else {
        p2Score++;
        iFrom = 0;
        iTo = (NUM_LEDS / 2);
        pickPlayer = 1;
      }
      ledColour = CRGB::Green;
      
    } else if (playMode == 1) { // React
      if (win) {
        ledColour = CRGB::Green;
        if (reactP1 == LOW && reactP2 == HIGH) { // Player 1 wins
          p1Score++;
          iFrom = NUM_LEDS / 2;
          iTo = NUM_LEDS;
        } else if (reactP1 == HIGH && reactP2 == LOW) { // Player 2 wins
          p2Score++;
          iFrom = 0;
          iTo = (NUM_LEDS / 2);
        } else { // Tie
          iFrom = 0;
          iTo = NUM_LEDS;
          ledColour = CRGB::Yellow;
        }
        
      } else {
        ledColour = CRGB::Red;
        if (!bPlayer1Press && !bPlayer2Press) {
          iFrom = 0;
          iTo = NUM_LEDS;
          p1Score--;
          p2Score--;
        }
        
        if (bPlayer1Press) {
          p1Score--;
          iFrom = NUM_LEDS / 2;
          iTo = NUM_LEDS;
        }
        if (bPlayer2Press) {
          p2Score--;
          iFrom = 0;
          if (!bPlayer1Press) {
            iTo = NUM_LEDS / 2;
          }
        }
      }
      pickPlayer = 99;
  
    } else if (playMode == 2) { // Tug-O-War
      if ((ledPos == 0 || (ledPos >= 250 && ledPos <= 255))) {
        p2Score++;
        iFrom = 0;
        iTo = (NUM_LEDS / 2);
        ledColour = CRGB::Green;
      } else if ((ledPos >= NUM_LEDS - 2 && ledPos <= NUM_LEDS + 5)) {
        p1Score++;
        iFrom = NUM_LEDS / 2;
        iTo = NUM_LEDS;
        ledColour = CRGB::Green;
      }
      
    } else if (playMode == 3) { // Minigolf
      iFrom = 0;
      iTo = NUM_LEDS;
      if (win) {
        p1Score++;
        ledColour = CRGB::Green;
      } else {
        ledColour = CRGB::Red;
      }
    }
    
    // Light up the LEDs
    for (byte i = iFrom; i < iTo; i++) {
      leds[i] = ledColour;
    }

    FastLED.setBrightness(ledBrightness);
    FastLED.show();
  
    delay(1000);

    // Check if any player has reached the winning goal (if there is one)
    if (firstTo > 0 && (p1Score == firstTo || p2Score == firstTo)) {
      String winnerStr;
      if (p1Score > p2Score) {
        winnerStr = "Player 1 wins!";
      } else {
        winnerStr = "Player 2 wins!";
      }
  
      menuDrawFn("", winnerStr, "", "Mode to restart");
  
      ledHue = 0;

      // Make rainbow colours travel across the LED strip towards the winning player
      while (!stopPlay) {
        // Press the mode button to end
        if (digitalRead(bMode) == LOW) {
          if (!bModePress) {
            bModePress = true;
            bModeEventMS = millis();
          }
      
          // Save the duration the button is pressed
          bModePressDurationMS = millis() - bModeEventMS;
      
        } else if (digitalRead(bMode) == HIGH && bModePress) { // Button released (or never pressed)
          if (bModePressDurationMS > debounceTime) { // Debounce duration check
            stopPlay = true;
            bModePress = false;
            bModeEventMS = 0;
            bModePressDurationMS = 0;
          }
        }
        
        for (byte i = 0; i < NUM_LEDS; i++) {
          if (p1Score > p2Score) {
            leds[i] = CHSV(ledHue - (i * 10), 255, ledBrightness);
          } else {
            leds[i] = CHSV(ledHue + (i * 10), 255, ledBrightness);
          }
        }
      
        FastLED.show();

        ledHue++;
      }
    }
  }

  FastLED.clear();
  FastLED.show();

  if (stopPlay) {
    drawMenu = true;
    menuItem = 0;
    startPlay = false;
    stopPlay = false;
    p1Score = 0;
    p2Score = 0;
    reactPress1Tot = 0;
    reactPress2Tot = 0;
    totalRounds = 0;
    pickPlayer = 99;
  }

  buttonBlockMS = 0;
  buttonBlockMS = 0;
  bPlayer1Press = false;
  bPlayer2Press = false;
  countP1 = 0;
  countP2 = 0;
  reactP1 = HIGH;
  reactP2 = HIGH;
  reactPress = false;
  reactPress1 = 0;
  reactPress2 = 0;
  playerCount = 0;
  chargeUpTime = 0;
  minigolfPlay = false;
  stopBeginLoop = false;
  beforeStart = true;
  rainbowPulse = false;
}

 /* Various */
// Random integer generator
int randomInt(int first, int last) {
  randomSeed(analogRead(A0) * (millis() % 10 + ((millis() / 10) % 10) * 10 + ((millis() / 100) % 10) * 100 + ((millis() / 1000) % 10) * 1000)); // Randomise the seed using unused A0 and the seconds part of millis()
  return random(first, last);
}

// Count number of digits in an integer
int countDigits(int in) {
  int numDigits = 0;
  
  if (in <= 0) { // Add one to number of digits if input is zero or less (to account for the negative indicator)
    numDigits++;
  }

  in = abs(in); // Deal with negative numbers
  
  while (in != 0) {
    in = in / 10;
    numDigits++;
  }

  return numDigits;
}

/*************
 *   Setup   *
 *************/

void setup() {
  /* OLED display */
  display.begin(&Adafruit128x32, SCREEN_ADDRESS, OLED_RESET);
  display.setFont(cp437font8x8); // https://en.wikipedia.org/wiki/Code_page_437
  display.clear();
  display.invertDisplay(true);
  display.set2X();
  display.setCursor((SCREEN_WIDTH - (strlen("Hi") * fontW)) / 2,1.5);
  display.print(F("Hi"));
  display.set1X();
  
  /* LEDs */
  FastLED.addLeds<WS2812, 8, GRB>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );

  /* Buttons */
  pinMode(bPlayer1, INPUT_PULLUP);
  pinMode(bPlayer2, INPUT_PULLUP);
  pinMode(bLeft, INPUT_PULLUP);
  pinMode(bRight, INPUT_PULLUP);
  pinMode(bMode, INPUT_PULLUP);

  /* Startup lights */
  byte i = 0, hue = randomInt(0,256), totI = 0, prevLED = 0;
  unsigned long ledDelayMS = millis();
  bool ledUp = true;
  while (totI < (NUM_LEDS * 2) - 1) {
    if (millis() - ledDelayMS >= 15) {
      leds[prevLED] = CRGB::Black;
      leds[i] = CHSV(hue + (i * 10), 255, 255);
      
      if (i == 0) {
        ledUp = true;
      } else if (i == NUM_LEDS - 1) {
        ledUp = false;
      }
      
      if (ledUp) {
        prevLED = i;
        i++;
      } else {
        prevLED = i;
        i--;
      }
      
      FastLED.show();
      
      totI++;
      
      ledDelayMS = millis();
    }
  }
  FastLED.clear();
  FastLED.show();

  display.clear();
  display.invertDisplay(false);
}

/************
 *   Loop   *
 ************/

void loop() {
  /* Show menu */
  if (!startPlay) {
    if (drawMenu) {
      drawMenu = false;
      
      if (menuItem == 0) { // Start menu
        menuDrawFn("LED Tennis v1.1", gameMode[playMode], "P1 - start", "Mode - settings");
        
      } else if (menuItem == 1) { // Pick play mode
        menuDrawFn("Play mode", gameMode[playMode], "P1 - set", "Menu");
        
      } else if (menuItem == 2) { // Set difficulty level
        if (playMode != 3) { // Don't show player 2 during 1-player game
          menuDrawFn("Difficulty", "P2: "+String(difficultyP2)+"    P1: "+String(difficultyP1), "P1 - set", "Menu");
        } else {
          menuDrawFn("Difficulty", "P1: "+String(difficultyP1), "P1 - set", "Menu");
        }

      } else if (menuItem == 3) { // Set winning goal (by default there's no goal)
        menuDrawFn("Winning goal", "First to "+String(firstTo), "P1 - set", "Menu");
        
      } else if (menuItem == 4) { // Set LED brightness
        menuDrawFn("LED Brightness", String(map(ledBrightness, minBright, maxBright, 1, 10)), "P1 - set", "Menu");
      }
    }
  }

  /* Buttons */
  // Player 1 button pressed
  if (digitalRead(bPlayer1) == LOW) {
    if (!bPlayer1State) {
      minigolfPlay = true;
      bPlayer1State = true;
      bPlayer1EventMS = millis();
    }

    bPlayer1PressDurationMS = millis() - bPlayer1EventMS;
    chargeUpTime = bPlayer1PressDurationMS; // Save value for play mode 3 (Minigolf)
    
  } else {
    if (bPlayer1State && bPlayer1PressDurationMS > debounceTime) {
      bPlayer1Press = true;
      bPlayer1State = false;
      bPlayer1EventMS = 0;
      bPlayer1PressDurationMS = 0;
    }
  }
  // Player 2 button pressed
  if (digitalRead(bPlayer2) == LOW) {
    if (!bPlayer2State) {
      bPlayer2State = true;
      bPlayer2EventMS = millis();
    }

    bPlayer2PressDurationMS = millis() - bPlayer2EventMS;
  } else {
    if (bPlayer2State && bPlayer2PressDurationMS > debounceTime) {
      bPlayer2Press = true;
      bPlayer2State = false;
      bPlayer2EventMS = 0;
      bPlayer2PressDurationMS = 0;
    }
  }

  // Player button events
  if (bPlayer1Press || bPlayer2Press) {
    if (startPlay) {
      if (!beforeStart) {
        // Tennis mode
        if (playMode == 0) {
          // Player 1 button pressed
          if (pickPlayer == 0 && bPlayer1Press) {
            if (difficultyP1 == 1 || buttonBlockMS == 0 || millis() - buttonBlockMS > 1000) { // Don't let player spam the button (unless on difficulty level 1)
              tennisShot(50 + difficultyP1, NUM_LEDS, 59, false);
            }

          // Player 2 button pressed
          } else if (pickPlayer == 1 && bPlayer2Press) {
            if (difficultyP2 == 1 || buttonBlockMS == 0 || millis() - buttonBlockMS > 1000) { // Don't let player spam the button (unless on difficulty level 1)
              tennisShot(0, 9 - difficultyP2, 0, true);
            }
          }

        // Tug-o-war mode
        } else if (playMode == 2) {
          // Count the number of presses
          if (bPlayer1Press) {
            reactPress1++;
          }
          if (bPlayer2Press) {
            reactPress2++;
          }
        }
      }
      
    } else {
      if (bPlayer1Press) {
        if (menuItem > 0) { // Go back to start menu
          menuItem = 0;
          
        } else { // Start game if on start menu
          startPlay = true;
        }
        
        drawMenu = true;
      }
    }

    bPlayer1Press = false;
    bPlayer2Press = false;
  }
  
  // Left or right button pressed (only active before game has started or in the pause menu)
  if (!startPlay && (digitalRead(bLeft) == LOW || digitalRead(bRight) == LOW)) {
    if (digitalRead(bLeft) == LOW) {
      if (!bLeftPress && !bRightPress) {
        bLeftPress = true;
        bChangeEventMS = millis();
      }
    } else if (digitalRead(bRight) == LOW) {
      if (!bRightPress && !bLeftPress) {
        bRightPress = true;
        bChangeEventMS = millis();
      }
    }

    // Save the duration the button is pressed
    bChangePressDurationMS = millis() - bChangeEventMS;

  } else if (!startPlay && (digitalRead(bLeft) == HIGH && bLeftPress || digitalRead(bRight) == HIGH && bRightPress)) { // Button released (or never pressed)
    if (bChangePressDurationMS > debounceTime) { // Debounce duration check
      // Change game mode
      if (menuItem == 1) {
        if (bLeftPress && playMode == 0) {
          playMode = numGames - 1;
        } else if (bRightPress && playMode == numGames - 1) {
          playMode = 0;
        } else {
          if (bLeftPress) {
            playMode--;
          } else if (bRightPress) {
            playMode++;
          }
        }

      // Change player difficulty
      } else if (menuItem == 2) {
        if (bLeftPress) {
          difficultyP2 = incrementDiff(difficultyP2);
        } else if (bRightPress) {
          difficultyP1 = incrementDiff(difficultyP1);
        }

      // Change winning goal (default none)
      } else if (menuItem == 3) {
        if (bLeftPress) {
          if (firstTo > 0) {
            firstTo--;
          }
          
        } else if (bRightPress) {
          firstTo++;
        }
      
      // Change LED brigthness
      } else if (menuItem == 4) {
        if (bLeftPress) {
          if (ledBrightness > minBright) {
            ledBrightness -= (maxBright - minBright) / 10;
          }
          
        } else if (bRightPress) {
          if (ledBrightness < maxBright) {
            ledBrightness += (maxBright - minBright) / 10;
          }
        }
      }

      drawMenu = true;
      
      bLeftPress = false;
      bRightPress = false;
      bChangeEventMS = 0;
      bChangePressDurationMS = 0;
    }
  }

  // Mode button pressed (only active before game)
  if (!startPlay && digitalRead(bMode) == LOW) {
    if (!bModePress) {
      bModePress = true;
      bModeEventMS = millis();
    }

    // Save the duration the button is pressed
    bModePressDurationMS = millis() - bModeEventMS;

  } else if (!startPlay & digitalRead(bMode) == HIGH && bModePress) { // Button released (or never pressed)
    if (bModePressDurationMS > debounceTime) { // Debounce duration check
      if (menuItem > 0) { // Change between settings menu screens
        if (menuItem == menuMax) {
          menuItem = 1;
        } else {
          menuItem++;
        }

        if (playMode == 3 && menuItem == 3) { // Don't show the "Winning goal" menu if "Minigolf" is the active game mode
          menuItem++;
        }
        
      } else { // Enter settings menu
        menuItem = 1;
      }
      
      drawMenu = true;
      
      bModePress = false;
      bModeEventMS = 0;
      bModePressDurationMS = 0;
    }
  }

  /* Play the game */
  if (startPlay) {
    if (beforeStart) {
      // Calcuale positions for score and other stats
      if (playMode == 3) { // Move the score for "Minigolf"
        scoreP1x = rightButtonMarker - fontW * 2 - (((countDigits(p1Score) - 1) * fontW) / 2);
      } else {
        scoreP1x = rightButtonMarker - (((countDigits(p1Score) - 1) * fontW) / 2);
      }
      scoreP2x = leftButtonMarker - (((countDigits(p2Score) - 1) * fontW) / 2);
      clicksP1x = rightButtonMarker - (((countDigits(reactPress1Tot) - 1) * fontW) / 2);
      clicksP2x = leftButtonMarker - (((countDigits(reactPress2Tot) - 1) * fontW) / 2);
      totalRoundsX = leftButtonMarker + fontW * 2 - (((countDigits(totalRounds) - 1) * fontW) / 2);
      if (playMode == 2) {
        scoreY = 2;
      } else {
        scoreY = 3;
      }
      clicksY = 3;

      // Show game mode and player stats (only update the score parts)
      if (drawMenu) {
        drawMenu = false;
        display.clear();
        display.setCursor((SCREEN_WIDTH - (strlen(gameMode[playMode]) * fontW)) / 2,0);
        display.print(gameMode[playMode]);
        if (playMode == 3) { // Screen for the Minigolf singel player game mode
          display.setCursor((SCREEN_WIDTH - (strlen("#Tries  Points") * fontW)) / 2,scoreY - 1);
          display.print(F("#Tries  Points"));
        } else { // Screen for two player game modes
          display.setCursor((SCREEN_WIDTH - (strlen("P2          P1") * fontW)) / 2,scoreY - 1);
          display.print(F("P2          P1"));
        }
      } else {
        display.clear(0, 127, scoreY, 3);
      }

      if (playMode == 3) { // Screen for the Minigolf singel player game mode
        display.setCursor(totalRoundsX,scoreY);
        display.print(totalRounds);
      } else { // Screen for two player game modes
        display.setCursor(scoreP2x,scoreY);
        display.print(p2Score);
      }
      if (playMode == 0 && totalRounds > 0) { // Display the number of "ball" hits for the last round (including the serve)
        display.setCursor((SCREEN_WIDTH - (strlen("#Hits") * fontW)) / 2,scoreY - 1);
        display.print(F("#Hits"));
        display.setCursor((SCREEN_WIDTH - (strlen(totalRounds) * fontW)) / 2,scoreY);
        display.print(totalRounds);
      } else if (playMode != 3) {
        display.setCursor((SCREEN_WIDTH - (strlen("-Score-") * fontW)) / 2,scoreY);
        display.print(F("-Score-"));
      }
      display.setCursor(scoreP1x,scoreY);
      display.print(p1Score);
      if (playMode == 2 && (reactPress1Tot > 0 || reactPress2Tot > 0)) { // Show the number of total clicks made during the game so far in the Tug-O-War game mode
        display.setCursor(clicksP1x,clicksY);
        display.print(reactPress1Tot);
        display.setCursor(clicksP2x,clicksY);
        display.print(reactPress2Tot);
        display.setCursor((SCREEN_WIDTH - (strlen("#Clicks") * fontW)) / 2,clicksY);
        display.print(F("#Clicks"));
      }

      // Pick a player (if applicable)
      if (pickPlayer > 1) {
        if (playMode < 3) {
          pickPlayer = randomInt(0,100) % 2;
        } else {
          pickPlayer = 1; 
        }
      }

      // Pulse lights to prepare for start
      ledBright = 1; // Led brigthness variable for fading the light up and down (start at 1 for the count variable)
      ledHue = 160; // Start the LEDs at a blue colour
      ctrlCount = 0; // Count the number of times the loop has run
      randomDelayMS = randomInt(1000,10000); // Set a delay until the React LEDs lights up (in ms)
      markPos = randomInt(0 + (6 - (difficultyP1 - 1)),(NUM_LEDS - 1) - ((NUM_LEDS / 2) / difficultyP1)); // Decide the position of the mark to hit in "Minigolf", based on the player difficulty level
      
      FastLED.clear();
      FastLED.setBrightness(ledBrightness);

      while (!stopBeginLoop) {
        // Blue light pulse at "active" player's end
        if (!rainbowPulse) {
          iFrom = pickPlayer * (NUM_LEDS - 5);
          iTo = iFrom + 5;
          ledFade = true;
          if (playMode == 3) {
            markPosition(markPos, NUM_LEDS); // Use NUM_LEDS as "ball" position as a placeholder
          }

          for (byte i = iFrom; i < iTo; i++) {
            leds[i] = CHSV(ledHue,255,ledBright);
          }
        
          FastLED.show();
        
          // Check if fade should increase or decrease
          if (ledBright == ledBrightness) {
            ledUp = false;
          } else if (ledBright == 0) {
            ledUp = true;
          }
          
          if (ledUp) {
            ledBright++;
          } else {
            ledBright--;
          }

        // Light pulse for the "React" game mode
        } else {
          for (byte i = 0; i < NUM_LEDS; i++) {
            if (i > NUM_LEDS / 2 - 1) {
              leds[i] = CHSV(ledHue - (i * 10), 255, ledBrightness);
            } else {
              leds[i] = CHSV(ledHue + (i * 10), 255, ledBrightness);
            }
          }
        
          FastLED.show();
  
          ledHue++;
        }

        // Check for player button inputs
        if (playMode != 3) {
          // Player 1 button pressed
          if (digitalRead(bPlayer1) == LOW) {
            if (!bPlayer1State) {
              bPlayer1State = true;
              bPlayer1EventMS = millis();
            }
        
            bPlayer1PressDurationMS = millis() - bPlayer1EventMS;
          } else {
            if (bPlayer1State && bPlayer1PressDurationMS > debounceTime) {
              bPlayer1Press = true;
              bPlayer1State = false;
              bPlayer1EventMS = 0;
              bPlayer1PressDurationMS = 0;
            }
          }
          // Player 2 button pressed
          if (digitalRead(bPlayer2) == LOW) {
            if (!bPlayer2State) {
              bPlayer2State = true;
              bPlayer2EventMS = millis();
            }
        
            bPlayer2PressDurationMS = millis() - bPlayer2EventMS;
          } else {
            if (bPlayer2State && bPlayer2PressDurationMS > debounceTime) {
              bPlayer2Press = true;
              bPlayer2State = false;
              bPlayer2EventMS = 0;
              bPlayer2PressDurationMS = 0;
            }
          }
        }

        // The different conditions to end the start pulse for each play mode
        if ((!rainbowPulse && (pickPlayer == 1 && bPlayer1Press || pickPlayer == 0 && bPlayer2Press)) || (rainbowPulse && playMode == 1 && (millis() - ledTimerMS > randomDelayMS || (bPlayer1Press || bPlayer2Press))) || (playMode == 3 && digitalRead(bPlayer1) == LOW)) {
          if (playMode == 1 && !rainbowPulse) {
            rainbowPulse = true;
            
            ledHue = randomInt(0,256); // Start the LEDs at a random colour
            
            bPlayer1Press = false; // Reset button detection
            bPlayer2Press = false;
            
            ledTimerMS = millis();
          } else {
            stopBeginLoop = true;
            beforeStart = false;
          }
          
        } else if (digitalRead(bMode) == LOW) { // Open the pause/end game menu
          stopBeginLoop = true;
          pauseYesNo = true;
        }
      }

      FastLED.clear();
      FastLED.setBrightness(ledBrightness);
      
      if (playMode == 0) { // "Tennis" setup
        ledPos = pickPlayer * (NUM_LEDS - 1);
        prevPos = ledPos;
        
        p1LEDDelay = baseDifficulty / difficultyP1;
        p2LEDDelay = baseDifficulty / difficultyP2;
        
        if (pickPlayer == 0) { // Set initial LED change delay for "Tennis"
          ledDelay = p1LEDDelay;
          ledUp = true;
        } else if (pickPlayer == 1) {
          ledDelay = p2LEDDelay;
          ledUp = false;
        }

        totalRounds = 1; // Reset number of "ball" hits (set it to 1 to account for the serve)

        // Hide the #Hits entry when playing
        display.clear((SCREEN_WIDTH - (strlen("#Hits") * fontW)) / 2 - fontW,((SCREEN_WIDTH - (strlen("#Hits") * fontW)) / 2) + (strlen("#Hits") * fontW) + fontW,scoreY - 1,scoreY);
        
      } else if (playMode == 2) { // "Tug-O-War" setup
        ledDelay = 1500; // The time-frame to measure button clicks in "Tug-o-war"
        ctrlCount = 0;
        ledPos = NUM_LEDS / 2 - 1;
        prevPos = ledPos;

        // Reset #Clicks
        reactPress1Tot = 0;
        reactPress2Tot = 0;

        // Hide the #Clicks entry when playing
        display.clear(0,127,clicksY,clicksY);

      } else if (playMode == 3) { // "Minigolf" setup
        ledPos = NUM_LEDS - 1;
        prevPos = ledPos;
        minigolfPlay = false;
        chargeCharging = 1;
      }
      
      // Reset button press detection on non-React game modes
      if (playMode != 1) {
        bPlayer1Press = false;
        bPlayer2Press = false;
      }
      
      ledTimerMS = millis();
    }

    // Pause/end game menu
    if (pauseYesNo) {
      FastLED.clear();
      FastLED.show();

      // Draw pause screen
      display.clear();
      display.setCursor((SCREEN_WIDTH - (strlen("Paused") * fontW)) / 2,0);
      display.print("Paused");
      display.setCursor((SCREEN_WIDTH - (strlen("End game?") * fontW)) / 2,2);
      display.print("End game?");
      display.setCursor((SCREEN_WIDTH - (strlen("Yes         No") * fontW)) / 2,3);
      display.print("Yes         No");

      // Wait for button input (we don't need a debounce button check here)
      while (pauseYesNo) {
        if (digitalRead(bLeft) == LOW) { // Press left button to end game
          pauseYesNo = false;
          beforeStart = true;
          stopPlay = true;
          gameEnd(false);
        } else if (digitalRead(bRight) == LOW) { // Press right button to resume game
          pauseYesNo = false;
          beforeStart = true;
          stopBeginLoop = false;
          drawMenu = true;
        }
      }
    }
    
    if (!beforeStart) {
      // Tennis (a single LED travels like a tennisball between the players who sends it back by pressing their button when the LED reaches them)
      if (playMode == 0) {
        if (millis() - ledTimerMS >= ledDelay) { // Update the LED position when the time limit has been reached
          if (ledPos >= NUM_LEDS && ledPos <= 255) { // Check if game has ended
            leds[prevPos] = CRGB::Black;
            gameEnd(true);
          } else {
            leds[prevPos] = CRGB::Black;
            leds[ledPos] = CRGB::White;
          
            FastLED.show();

            prevPos = ledPos; // Save current LED position so that it can be turned off next loop

            // Move LED up or down the strip
            if (ledUp) {
              ledPos++;
            } else {
              ledPos--;
            }
          }
            
          ledTimerMS = millis();
        }
        
      // React (players need to press their button quickly when the LED strip stops pulsing after a random time interval)
      } else if (playMode == 1) {
        // Buttons have been pressed too early. Points will be deducted.
        if ((bPlayer1State || bPlayer2State) || (bPlayer1Press || bPlayer2Press)) {
          if (bPlayer1State || bPlayer1Press) {
            bPlayer1Press = true;
            bPlayer1State = false;
            bPlayer1EventMS = 0;
            bPlayer1PressDurationMS = 0;
          }
          if (bPlayer2State || bPlayer2Press) {
            bPlayer2Press = true;
            bPlayer2State = false;
            bPlayer2EventMS = 0;
            bPlayer2PressDurationMS = 0;
          }
          gameEnd(false);
          
        } else {
          // Check player difficulty levels and score. LEDs will stay on for a maximum time based on the whichever players difficulty level plus score is lowest
          if (difficultyP1 + p1Score <= difficultyP2 + p2Score) {
            if (p1Score < 0 && abs(p1Score) >= difficultyP1) { // Make sure that we do not divide by zero or get a negative difficulty multiplier
              difficultyMultiplier = 1;
            } else {
              difficultyMultiplier = difficultyP1 + p1Score;
            }
            
          } else {
            if (p2Score < 0 && abs(p2Score) >= difficultyP2) { // Make sure that we do not divide by zero or get a negative difficulty multiplier
              difficultyMultiplier = 1;
            } else {
              difficultyMultiplier = difficultyP2 + p2Score;
            } 
          }
  
          fill_solid(leds, NUM_LEDS, CRGB::White); // Light the whole strip in a solid colour
          FastLED.show();
          
          ledTimerMS = millis();
  
          // Check timing of button presses, depends on player difficulty and score
          while (millis() - ledTimerMS < (ledFadeTimer / difficultyMultiplier) && !reactPress) {
            reactP1 = digitalRead(bPlayer1);
            reactP2 = digitalRead(bPlayer2);
            if ((reactP1 == LOW && millis() - ledTimerMS < (ledFadeTimer / (difficultyP1 + p1Score))) || (reactP2 == LOW && millis() - ledTimerMS < (ledFadeTimer / (difficultyP2 + p2Score)))) {
              reactPress = true;
            }
          }
  
          // Check if a player scored
          if (reactP1 == LOW || reactP2 == LOW) { // Someone pressed their button in time
            gameEnd(true);
            
          } else { // Time ran out before either button was pressed
            gameEnd(false);
          }
        }
        
      // Tug-o-war (players need to press the button multiple times to move two LEDs to their side of the strip)
      } else if (playMode == 2) {
        // Flash the LEDS red-yellow-green to start the game
        if (ctrlCount == 0) {
          fill_solid(leds, NUM_LEDS, CHSV(48 * ctrlCount,255,ledBrightness));
          FastLED.show();
          
          while (ctrlCount < 3) {
            if (millis() - ledTimerMS > 1000) {
              ctrlCount++;
  
              if (ctrlCount == 3) {
                // Light up center LEDs, that are "pulled" by the players
                FastLED.clear();
                centerLEDs(ledPos, CRGB::White);
              } else {
                fill_solid(leds, NUM_LEDS, CHSV(48 * ctrlCount,255,ledBrightness));
              }
              FastLED.show();
                
              ledTimerMS = millis();
            }
          }
        }

        // Check mode button for pause/end game menu (must be here since reaching the "before game" state in this game mode isn't as easy as the others)
        if (digitalRead(bMode) == LOW) { // Open the pause/end game menu
          pauseYesNo = true;
        }

        // Finish the round when the LED position reaches one of the ends
        if ((ledPos == 0 || (ledPos >= 250 && ledPos <= 255)) || (ledPos >= NUM_LEDS - 2 && ledPos <= NUM_LEDS + 5)) {
          gameEnd(true);
          if (pickPlayer == 1) {
            pickPlayer == 0;
          } else {
            pickPlayer == 1;
          }

        // Check which player has the most clicks in the measured time and adjust LED position accordingly (based on player difficulty settings)
        } else {
          if (millis() - ledTimerMS > ledDelay) {
            // Find new LED position
            if (reactPress1 - reactPress2 > 0) { // Player 1 has the most clicks
              if ((NUM_LEDS - 1) - ledPos < (difficultyLevels - (difficultyP1 - 1))) {
                ledPos = NUM_LEDS - 1;
              } else {
                ledPos += difficultyLevels - (difficultyP1 - 1);
              }
              
            } else if (reactPress1 - reactPress2 < 0) { // Player 2 has the most clicks
              if (ledPos < (difficultyLevels - (difficultyP2 - 1))) {
                ledPos = 0;
              } else {
                ledPos -= difficultyLevels - (difficultyP2 - 1);
              }
            }

            // Change the LED colour from white to green the closer it gets to one end of the strip
            if (ledPos >= (NUM_LEDS / 2) - 10 && ledPos < (NUM_LEDS / 2) + 10) {
              ledColour = CRGB::White;
            } else {
              if (ledPos >= (NUM_LEDS / 2) + 10) {
                ledHue = 96 - 1.5 * (NUM_LEDS - 1 - ledPos);
              } else {
                ledHue = 96  - 1.5 * ledPos;
              }
              ledColour = CHSV(ledHue,255,ledBrightness);
            }

            // Move the LEDs to the new position, one step at a time (but rather quickly)
            if (ledPos > prevPos) {
              for (byte i = prevPos; i < ledPos; i++) {
                FastLED.clear();
                leds[i] = ledColour;
                FastLED.show();
                delay(10);
              }
            } else {
              for (byte i = prevPos; i > ledPos; i--) {
                FastLED.clear();
                leds[i] = ledColour;
                FastLED.show();
                delay(10);
              }
            }
            
            FastLED.clear();
            centerLEDs(ledPos, ledColour);
            FastLED.show();

            prevPos = ledPos; // Save current LED position

            // Count presses and reset
            reactPress1Tot += reactPress1;
            reactPress2Tot += reactPress2;
            reactPress1 = 0;
            reactPress2 = 0;

            ledTimerMS = millis();
          }
        }

      // Minigolf (1-player game where the player needs to charge up their light and make it hit a random LED light on the strip) 
      } else if (playMode == 3) {
        // Update the charge LEDs while button is pressed, until maximum charge time is reached (based on the total number of LEDs)
        if (minigolfPlay) {
          if (bPlayer1State && bPlayer1PressDurationMS < chargeMax) {
            leds[NUM_LEDS - 1] = CHSV(96,255,ledBrightness); // Light up the first charge LED
            leds[(NUM_LEDS - 1) - chargeMax / 1000] = CHSV(0,255,ledBrightness); // Light up the charge max LED
            markPosition(markPos, NUM_LEDS); // Use NUM_LEDS as "ball" position as a placeholder
            FastLED.show();

            if (bPlayer1PressDurationMS > 1000 * chargeCharging) {
              for (byte i = NUM_LEDS - 1; i >= (NUM_LEDS - 1) - chargeCharging; i--) {
                leds[i] = CHSV(96 - (16 * ((NUM_LEDS - 1) - i)),255,ledBrightness);
              }
              FastLED.show();
    
              chargeCharging++;
            }
  
          // Release the "ball"
          } else {
            FastLED.clear();
            markPosition(markPos, NUM_LEDS); // Use NUM_LEDS as "ball" position as a placeholder
            FastLED.show();
  
            chargeTo = chargeUpTime / 100; // Find the number of LEDs to travel
            if (chargeTo > (NUM_LEDS - 1)) { // If the button was released too late or not at all, set the mark at the end LED
              chargeTo = (NUM_LEDS - 1);
            }
            
            ledDelay = 10;

            // Move the "ball"
            while (ledPos >= ((NUM_LEDS - 1) - chargeTo - 1) && ledPos != 0) {
              if (millis() - ledTimerMS > ledDelay || (ledColour == CRGB(0,0,255) && millis() - ledTimerMS > 750)) {
                if (ledPos != ((NUM_LEDS - 1) - chargeTo)) {
                  ledColour = CRGB::White;
                } else {
                  ledColour = CRGB::Blue;
                }
                leds[prevPos] = CRGB::Black;
                leds[ledPos] = ledColour;
                markPosition(markPos,ledPos);
                FastLED.show();
    
                prevPos = ledPos;
                ledPos--;

                // Start slowing down as the "ball" gets closer, but only if the button was releasted in time
                if (chargeTo != (NUM_LEDS - 1)) {
                  if (ledPos - ((NUM_LEDS - 1) - chargeTo) <= 15) {
                    ledDelay = exp((15 - (ledPos - ((NUM_LEDS - 1) - chargeTo))) / 2); // Within 15 LEDs of the mark to hit, the slowdown is exponential
                    if (ledDelay < 10) {
                      ledDelay = 10;
                    }
                  }
                }
                ledTimerMS = millis();
              }
            }
  
            // Calculate if there was a "hit" (based on player difficulty level)
            if (((NUM_LEDS - 1) - chargeTo) >= markPos && ((NUM_LEDS - 1) - chargeTo) <= markPos + (difficultyLevels - difficultyP1)) {
              gameEnd(true);
            } else {
              gameEnd(false);
            }

            totalRounds++;
          }
        }
      }
    }
  }
}
