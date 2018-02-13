#include "dial.h"
#include "math.h" 
#include "GD2.h"

void DialClass::open() {
  dialog=true;  
  voltDecade = "V";
}

void DialClass::handleKeypad() {


if (dialog==false) {
  return;
}
int screenWidth = 800;
int screenHeight = 480;
int width = 530;
int height = 470;
int margin = 10;

int startx, starty, endx, endy;
startx=(screenWidth-width) / 2;
starty=(screenHeight-height) / 2;
endx=screenWidth - (screenWidth-width) / 2;
endy=screenHeight - (screenHeight-height) / 2;

GD.ColorRGB(0x000000);
GD.Begin(RECTS);
GD.Vertex2ii(startx, starty); 
GD.Vertex2ii(endx, endy);

GD.ColorRGB(0xaaaaaa);
GD.Begin(LINE_STRIP);
GD.LineWidth(32);
GD.Vertex2ii(startx, starty); 
GD.Vertex2ii(startx+width, starty); 
GD.Vertex2ii(startx+width, starty+height); 
GD.Vertex2ii(startx, starty+height); 
GD.Vertex2ii(startx, starty); 

int x=190;
int y=175;
int spacing = 82;
GD.Tag(1);
transButton(x+0, y+0,18, "1", 31); 
GD.Tag(2);
transButton(x+spacing, y+0, 18, "2", 31);
GD.Tag(3);
transButton(x+spacing*2, y+0, 18, "3", 31);
GD.Tag(KEYBOARD_V);
transButton(x+spacing*3, y+0, 18, "V", 31);
GD.Tag(KEYBOARD_BACK);
transButton(x+spacing*4, y+0, 18, "<", 31);

GD.Tag(4);
transButton(x+0, y+spacing, 18, "4", 31); 
GD.Tag(5);
transButton(x+spacing, y+spacing, 18, "5", 31); 
GD.Tag(6);
transButton(x+spacing*2, y+spacing, 18, "6", 31);
GD.Tag(KEYBOARD_MV);
transButton(x+spacing*3, y+spacing, 18, "mV", 30);
GD.Tag(KEYBOARD_CLR);
transButton(x+spacing*4, y+spacing, 18, "Clr", 30);

GD.Tag(7);
transButton(x+0, y+spacing*2, 18, "7", 31); 
GD.Tag(8);
transButton(x+spacing, y+spacing*2, 18, "8", 31); 
GD.Tag(9);
transButton(x+spacing*2, y+spacing*2, 18, "9", 31);
GD.Tag(KEYBOARD_UV);
transButton(x+spacing*3, y+spacing*2, 18, "uV", 30);
GD.Tag(KEYBOARD_DIVIDE10);
transButton(x+spacing*4, y+spacing*2, 18, "/10", 30);

GD.Tag(KEYBOARD_0);
transButton(x+0, y+spacing*3, 18, "0",31); 
GD.Tag(KEYBOARD_PLUSMINUS);
transButton(x+spacing, y+spacing*3, 18, "+/-",30);
GD.Tag(KEYBOARD_COMMA); 
transButton(x+spacing*2, y+spacing*3, 18, ".",31);
transButton(x+spacing*3, y+spacing*3, 18, " ", 31);
GD.Tag(KEYBOARD_TIMES10);
transButton(x+spacing*4, y+spacing*3, 18, "x10", 30);

GD.Tag(KEYBOARD_CANCEL);
transButton(x+spacing*5, y+spacing*2, 18, "Cancel", 28);
GD.Tag(KEYBOARD_OK);
transButton(x+spacing*5, y+spacing*3, 18, "OK", 31);



// entry display
GD.ColorRGB(0xaaaaaa);
GD.Begin(LINE_STRIP);
GD.LineWidth(16);
GD.Vertex2ii(startx+20, starty+20); 
GD.Vertex2ii(startx+width-20, starty+20); 
GD.Vertex2ii(startx+width-20, starty+110); 
GD.Vertex2ii(startx+20, starty+110); 
GD.Vertex2ii(startx+20, starty+20); 


  GD.get_inputs();
  


  if (GD.inputs.tag == KEYBOARD_CANCEL && dialog==true) {
    dialog = false;
  }

  if (GD.inputs.tag == KEYBOARD_OK && dialog==true) {
    dialog = false;
  }
  
  if (GD.inputs.tag != 0 && keydepressed==true) {
    keydepressed = false;
    if (digits > 0 && GD.inputs.tag == KEYBOARD_OK) {
    }
    if (digits == 0 && GD.inputs.tag == KEYBOARD_COMMA) {
      dialEntries[digits++] = 0;
    }
    if (GD.inputs.tag == KEYBOARD_COMMA) {
      bool alreadyHasComma = false;
      for (int i=0;i<digits;i++) {
        if (dialEntries[i] == KEYBOARD_COMMA) {
          alreadyHasComma = true;
          break;
        }
      }
      if (!alreadyHasComma) {
        dialEntries[digits++] = GD.inputs.tag;
      }

    }
    if (GD.inputs.tag >= KEYBOARD_1 && GD.inputs.tag <= KEYBOARD_9 && digits<8) {
      dialEntries[digits++] = GD.inputs.tag;
    }
    if (GD.inputs.tag == KEYBOARD_0 && digits<8) {
      dialEntries[digits++] = 0;
    }
    if (GD.inputs.tag== KEYBOARD_UV) {
      voltDecade = "uV";
    }
    if (GD.inputs.tag== KEYBOARD_MV) {
      voltDecade = "mV";
    }
    if (GD.inputs.tag== KEYBOARD_V) {
      voltDecade = "V";
    }
    if (GD.inputs.tag== KEYBOARD_BACK) {
      if (digits>0) {
        digits --;
      }
    }
    if (GD.inputs.tag== KEYBOARD_CLR) {
      digits = 0;
    }
    if (GD.inputs.tag== KEYBOARD_PLUSMINUS) {
      negative = !negative;
    }
  } 
  
  if (GD.inputs.tag == 0) {
    keydepressed = true;
  }

  
  /* Show input values */
  int posx = 170;
  int posy = 18;
  GD.ColorRGB(COLOR_VOLT);
  if (negative == true) {
      GD.cmd_text(posx, posy, 1, 0, "-");
  } else {
      GD.cmd_text(posx, posy, 1, 0, "+");
  }
  posx=posx+50;
  for (int i=0;i<digits;i++) {
    if (dialEntries[i] < KEYBOARD_COMMA) {
      GD.cmd_number(posx, posy, 1, 0, dialEntries[i]);
      posx=posx+50;
    }
    if (dialEntries[i]== KEYBOARD_COMMA) {
      GD.cmd_text(posx, posy, 1, 0, ".");
      posx=posx+20;
    }
    
    if (i==digits-1) {
      GD.cmd_text(posx+10, posy,1, 0, voltDecade);
    }
  }
}


void DialClass::transButton(int x, int y, int sz, char* label, int fontsize)
{
GD.Begin(RECTS);
GD.ColorA(200);
GD.ColorRGB(200,200,200);
GD.LineWidth(16 * 20);
GD.Vertex2ii(x - sz, y - sz);
GD.Vertex2ii(x + sz, y + sz);

GD.ColorA(200);
GD.ColorRGB(255,255,255);
GD.LineWidth(16 * 15);
GD.Vertex2ii(x - sz, y - sz);
GD.Vertex2ii(x + sz, y + sz);
GD.ColorA(0xff);
GD.ColorRGB(0,0,0);
GD.cmd_text(x, y, fontsize, OPT_CENTER, label);
GD.ColorA(255);

}


void DialClass::clear() {
  digits = 0;
}

double DialClass::toMv() {
/* calculate mv */
 int dec = 0;
 double sum = 0;
 for (int i=0;i<digits;i++) {
   if (dialEntries[i] == KEYBOARD_COMMA) {
     dec=1;
     i++;
   }
   if (dec==0) {
     if (sum==0){ // first
       sum = dialEntries[i];
     } else {
       sum = sum*10 + dialEntries[i];
     }
   } else {
      double decValue = dialEntries[i]/pow(10,dec);
      sum = sum + decValue;
      dec++;
   }
 }
 if (voltDecade == "V") {
   sum=sum*1000;
 }
 if (voltDecade == "uV") {
   sum=sum/1000;
 }
 return sum;
}

DialClass DIAL;

