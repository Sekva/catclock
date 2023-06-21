#include <X11/ICE/ICElib.h>
#include <X11/X.h>
#include <math.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
 *  X11 includes
 */
#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/Xlib.h>
#include <X11/Xos.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>

/*
 *  Motif includes
 */
#include <Xm/DrawingA.h>
#include <Xm/Form.h>
#include <Xm/MenuShell.h>
#include <Xm/PushB.h>
#include <Xm/RowColumn.h>
#include <Xm/Separator.h>
#include <Xm/ToggleB.h>
#include <Xm/Xm.h>

/*
 *  Cat bitmap includes
 */
#include "graphics/bitmaps.h"

/*
 *  Cat body part pixmaps
 */
static Pixmap *eyePixmap = (Pixmap *)NULL;  /*  Array of eyes     */
static Pixmap *tailPixmap = (Pixmap *)NULL; /*  Array of tails    */

/*
 *  Cat GC's
 */
static GC catGC;  /*  For drawing cat's body    */
static GC tailGC; /*  For drawing cat's tail    */
static GC eyeGC;  /*  For drawing cat's eyes    */

/*
 *  Default cat dimension stuff -- don't change sizes!!!!
 */
#define DEF_N_TAILS 40     /*  Default resolution        */
#define TAIL_HEIGHT 89     /*  Tail pixmap height        */
#define DEF_CAT_WIDTH 150  /*  Cat body pixmap width     */
#define DEF_CAT_HEIGHT 300 /*  Cat body pixmap height    */
#define DEF_CAT_BOTTOM 210 /*  Distance to cat's butt    */

/*
 *  Clock hand stuff
 */
#define VERTICES_IN_HANDS 4  /*  Hands are triangles      */
#define SECOND_HAND_FRACT 90 /*  Percentages of radius    */
#define MINUTE_HAND_FRACT 70
#define HOUR_HAND_FRACT 40
#define HAND_WIDTH_FRACT 7
#define SECOND_WIDTH_FRACT 5

static int centerX = 0; /*  Window coord origin of      */
static int centerY = 0; /*  clock hands.                */

static int radius; /*  Radius of clock face        */

static int secondHandLength; /*  Current lengths and widths  */
static int minuteHandLength;
static int hourHandLength;
static int handWidth;
static int secondHandWidth;

#define SEG_BUFF_SIZE 128                  /*  Max buffer size     */
static int numSegs = 0;                    /*  Segments in buffer  */
static XPoint segBuf[SEG_BUFF_SIZE];       /*  Buffer              */
static XPoint *segBufPtr = (XPoint *)NULL; /*  Current pointer     */

/*
 *  Default font for digital display
 */
#define DEF_DIGITAL_FONT "fixed"

/*
 *  Padding defaults
 */
#define DEF_DIGITAL_PADDING 10 /*  Space around time display   */
#define DEF_ANALOG_PADDING 8   /*  Radius padding for analog   */

/*
 *  Time stuff
 */
static struct tm tm;  /*  What time is it?            */
static struct tm otm; /*  What time was it?           */

/*
 *  X11 Stuff
 */
static Window clockWindow = (Window)NULL;
static XtAppContext appContext;
Display *dpy;
Window root;
int screen;

GC gc;             /*  For tick-marks, text, etc.  */
static GC handGC;  /*  For drawing hands           */
static GC eraseGC; /*  For erasing hands           */
static GC highGC;  /*  For hand borders            */

/*
 *  Resources user can set in addition to normal Xt resources
 */
typedef struct {
  XFontStruct *font; /*  For alarm & analog  */

  Pixel foreground; /*  Foreground          */
  Pixel background; /*  Background          */

  Pixel highlightColor; /*  Hand outline/border */
  Pixel handColor;      /*  Hand interior       */
  Pixel catColor;       /*  Cat's body          */
  Pixel detailColor;    /*  Cat's paws, belly,  */
                        /*  face, and eyes.     */
  Pixel tieColor;       /*  Cat's tie color     */
  int nTails;           /*  Tail/eye resolution */

  int padding;      /*  Font spacing        */
  char *modeString; /*  Display mode        */

  int update;    /*  Seconds between     */
                 /*  updates             */
  Boolean chime; /*  Chime on hour?      */

  int help; /*  Display syntax      */

} ApplicationData, *ApplicationDataPtr;

static ApplicationData appData;

/*
 *  Miscellaneous stuff
 */

#define TWOPI (2.0 * M_PI) /*  2.0 * M_PI  */
#define UNINIT -1

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

GC CreateTailGC(void) {
  GC tailGC;
  XGCValues tailGCV;
  unsigned long valueMask;

  tailGCV.function = GXcopy;
  tailGCV.plane_mask = AllPlanes;
  tailGCV.foreground = appData.catColor;
  tailGCV.background = appData.background;
  tailGCV.line_width = 15;
  tailGCV.line_style = LineSolid;
  tailGCV.cap_style = CapRound;
  tailGCV.join_style = JoinRound;
  tailGCV.fill_style = FillSolid;
  tailGCV.subwindow_mode = ClipByChildren;
  tailGCV.clip_x_origin = 0;
  tailGCV.clip_y_origin = 0;
  tailGCV.clip_mask = None;
  tailGCV.graphics_exposures = False;

  valueMask = GCFunction | GCPlaneMask | GCForeground | GCBackground |
              GCLineWidth | GCLineStyle | GCCapStyle | GCJoinStyle |
              GCFillStyle | GCSubwindowMode | GCClipXOrigin | GCClipYOrigin |
              GCClipMask | GCGraphicsExposures;

  tailGC = XCreateGC(dpy, clockWindow, valueMask, &tailGCV);

  return (tailGC);
}

static GC CreateEyeGC(void) {
  GC eyeGC;
  XGCValues eyeGCV;
  unsigned long valueMask;

  eyeGCV.function = GXcopy;
  eyeGCV.plane_mask = AllPlanes;
  eyeGCV.foreground = appData.catColor;
  eyeGCV.background = appData.detailColor;
  eyeGCV.line_width = 15;
  eyeGCV.line_style = LineSolid;
  eyeGCV.cap_style = CapRound;
  eyeGCV.join_style = JoinRound;
  eyeGCV.fill_style = FillSolid;
  eyeGCV.subwindow_mode = ClipByChildren;
  eyeGCV.clip_x_origin = 0;
  eyeGCV.clip_y_origin = 0;
  eyeGCV.clip_mask = None;
  eyeGCV.graphics_exposures = False;

  valueMask = GCFunction | GCPlaneMask | GCForeground | GCBackground |
              GCLineWidth | GCLineStyle | GCCapStyle | GCJoinStyle |
              GCFillStyle | GCSubwindowMode | GCClipXOrigin | GCClipYOrigin |
              GCClipMask | GCGraphicsExposures;

  eyeGC = XCreateGC(dpy, clockWindow, valueMask, &eyeGCV);

  return (eyeGC);
}

Pixmap CreateTailPixmap(double t) {
  Pixmap tailBitmap;
  GC bitmapGC;
  double sinTheta, cosTheta; /*  Pendulum parameters */
  double A = 0.4;
  double omega = 1.0;
  double phi = 3 * M_PI_2;
  double angle;

  static XPoint tailOffset = {74, -15};

#define N_TAIL_PTS 7
  static XPoint tail[N_TAIL_PTS] = {
      /*  "Center" tail definition */
      {0, 0}, {0, 76}, {3, 82}, {10, 84}, {18, 82}, {21, 76}, {21, 70},
  };

  XPoint offCenterTail[N_TAIL_PTS]; /* off center tail    */
  XPoint newTail[N_TAIL_PTS];       /*  Tail at time "t"  */
  XGCValues bitmapGCV;              /*  GC for drawing    */
  unsigned long valueMask;
  int i;

  /*
   *  Create GC for drawing tail
   */
  bitmapGCV.function = GXcopy;
  bitmapGCV.plane_mask = AllPlanes;
  bitmapGCV.foreground = 1;
  bitmapGCV.background = 0;
  bitmapGCV.line_width = 15;
  bitmapGCV.line_style = LineSolid;
  bitmapGCV.cap_style = CapRound;
  bitmapGCV.join_style = JoinRound;
  bitmapGCV.fill_style = FillSolid;
  bitmapGCV.subwindow_mode = ClipByChildren;
  bitmapGCV.clip_x_origin = 0;
  bitmapGCV.clip_y_origin = 0;
  bitmapGCV.clip_mask = None;

  valueMask = GCFunction | GCPlaneMask | GCForeground | GCBackground |
              GCLineWidth | GCLineStyle | GCCapStyle | GCJoinStyle |
              GCFillStyle | GCSubwindowMode | GCClipXOrigin | GCClipYOrigin |
              GCClipMask;

  tailBitmap =
      XCreateBitmapFromData(dpy, root, tail_bits, tail_width, tail_height);
  bitmapGC = XCreateGC(dpy, tailBitmap, valueMask, &bitmapGCV);

  {
    /*
     *  Create an "off-center" tail to deal with the fact that
     *  the tail has a hook to it.  A real pendulum so shaped would
     *  hang a bit to the left (as you look at the cat).
     */
    angle = -0.08;
    sinTheta = sin(angle);
    cosTheta = cos(angle);

    for (i = 0; i < N_TAIL_PTS; i++) {
      offCenterTail[i].x = (int)((double)(tail[i].x) * cosTheta +
                                 (double)(tail[i].y) * sinTheta);
      offCenterTail[i].y = (int)((double)(-tail[i].x) * sinTheta +
                                 (double)(tail[i].y) * cosTheta);
    }
  }

  /*
   *  Compute pendulum function.
   */
  angle = A * sin(omega * t + phi);
  sinTheta = sin(angle);
  cosTheta = cos(angle);

  /*
   *  Rotate the center tail about its origin by "angle" degrees.
   */
  for (i = 0; i < N_TAIL_PTS; i++) {
    newTail[i].x = (int)((double)(offCenterTail[i].x) * cosTheta +
                         (double)(offCenterTail[i].y) * sinTheta);
    newTail[i].y = (int)((double)(-offCenterTail[i].x) * sinTheta +
                         (double)(offCenterTail[i].y) * cosTheta);

    newTail[i].x += tailOffset.x;
    newTail[i].y += tailOffset.y;
  }

  /*
   *  Create pixmap for drawing tail (and stippling on update)
   */
  XDrawLines(dpy, tailBitmap, bitmapGC, newTail, N_TAIL_PTS, CoordModeOrigin);

  XFreeGC(dpy, bitmapGC);

  return (tailBitmap);
}

void ParseGeometry(Widget topLevel) {
  int n;
  Arg args[10];
  char *geomString = NULL;
  char *geometry, xString[80], yString[80], widthString[80], heightString[80];

  /*
   *  Grab the geometry string from the topLevel widget
   */
  n = 0;
  XtSetArg(args[n], XmNgeometry, &geomString);
  n++;
  XtGetValues(topLevel, args, n);

  /*
   * Static analysis tags this as a memory leak...
   * Must be dynamically allocated; otherwise, the geom string
   * would be out of scope by the time XtRealizeWidget is called,
   * and you would get a garbage geometry specification, at best.
   */
  geometry = malloc(80);

  if (geomString == NULL) {
    /*
     *  User didn't specify any geometry, so we
     *  use the default.
     */
    sprintf(geometry, "%dx%d", DEF_CAT_WIDTH, DEF_CAT_HEIGHT);
  } else {
    /*
     *  Gotta do some work.
     */
    int x, y;
    unsigned int width, height;
    int geomMask;

    /*
     *  Find out what's been set
     */
    geomMask = XParseGeometry(geomString, &x, &y, &width, &height);

    /*
     *  Fix the cat width and height
     */
    sprintf(widthString, "%d", DEF_CAT_WIDTH);
    sprintf(heightString, "x%d", DEF_CAT_HEIGHT);

    /*
     *  Use the x and y values, if any
     */
    if (!(geomMask & XValue)) {
      strcpy(xString, "");
    } else {
      if (geomMask & XNegative) {
        sprintf(xString, "-%d", x);
      } else {
        sprintf(xString, "+%d", x);
      }
    }

    if (!(geomMask & YValue)) {
      strcpy(yString, "");
    } else {
      if (geomMask & YNegative) {
        sprintf(yString, "-%d", y);
      } else {
        sprintf(yString, "+%d", y);
      }
    }

    /*
     *  Put them all together
     */
    geometry[0] = '\0';
    strcat(geometry, widthString);
    strcat(geometry, heightString);
    strcat(geometry, xString);
    strcat(geometry, yString);
  }

  /*
   *  Stash the width and height in some globals (ugh!)
   */
  {
    int ww = DEF_CAT_WIDTH;
    int hh = DEF_CAT_HEIGHT;
    sscanf(geometry, "%dx%d", &ww, &hh);
  }

  /*
   *  Set the geometry of the topLevel widget
   */
  n = 0;
  XtSetArg(args[n], XmNwidth, DEF_CAT_WIDTH);
  n++;
  XtSetArg(args[n], XmNheight, DEF_CAT_HEIGHT);
  n++;
  XtSetArg(args[n], XmNminWidth, DEF_CAT_WIDTH);
  n++;
  XtSetArg(args[n], XmNminHeight, DEF_CAT_HEIGHT);
  n++;
  XtSetArg(args[n], XmNmaxWidth, DEF_CAT_WIDTH);
  n++;
  XtSetArg(args[n], XmNmaxHeight, DEF_CAT_HEIGHT);
  n++;
  XtSetArg(args[n], XmNgeometry, geometry);
  n++;
  XtSetValues(topLevel, args, n);
}

void SetSeg(int x1, int y1, int x2, int y2) {
  segBufPtr->x = x1;
  segBufPtr++->y = y1;
  segBufPtr->x = x2;
  segBufPtr++->y = y2;

  numSegs += 2;
}

/*
 *  DrawLine - Draws a line.
 *
 *  blankLength is the distance from the center which the line begins.
 *  length is the maximum length of the hand.
 *  fractionOfACircle is a fraction between 0 and 1 (inclusive) indicating
 *  how far around the circle (clockwise) from high noon.
 *
 *  The blankLength feature is because I wanted to draw tick-marks around the
 *  circle (for seconds).  The obvious means of drawing lines from the center
 *  to the perimeter, then erasing all but the outside most pixels doesn't
 *  work because of round-off error (sigh).
 */
void DrawLine(int blankLength, int length, double fractionOfACircle) {
  double angle, cosAngle, sinAngle;

  /*
   *  A full circle is 2 M_PI radians.
   *  Angles are measured from 12 o'clock, clockwise increasing.
   *  Since in X, +x is to the right and +y is downward:
   *
   *    x = x0 + r * sin(theta)
   *    y = y0 - r * cos(theta)
   *
   */
  angle = TWOPI * fractionOfACircle;
  cosAngle = cos(angle);
  sinAngle = sin(angle);

  SetSeg(centerX + (int)(blankLength * sinAngle),
         centerY - (int)(blankLength * cosAngle),
         centerX + (int)(length * sinAngle),
         centerY - (int)(length * cosAngle));
}

int Round(double x) { return (x >= 0.0 ? (int)(x + 0.5) : (int)(x - 0.5)); }

/*
 *  DrawHand - Draws a hand.
 *
 *  length is the maximum length of the hand.
 *  width is the half-width of the hand.
 *  fractionOfACircle is a fraction between 0 and 1 (inclusive) indicating
 *  how far around the circle (clockwise) from high noon.
 *
 */
void DrawHand(int length, int width, double fractionOfACircle) {
  double angle, cosAngle, sinAngle;
  double ws, wc;
  int x, y, x1, y1, x2, y2;

  /*
   *  A full circle is 2 PI radians.
   *  Angles are measured from 12 o'clock, clockwise increasing.
   *  Since in X, +x is to the right and +y is downward:
   *
   *    x = x0 + r * sin(theta)
   *    y = y0 - r * cos(theta)
   *
   */
  angle = TWOPI * fractionOfACircle;
  cosAngle = cos(angle);
  sinAngle = sin(angle);

  /*
   * Order of points when drawing the hand.
   *
   *        1,4
   *        / \
   *       /   \
   *      /     \
   *    2 ------- 3
   */
  wc = width * cosAngle;
  ws = width * sinAngle;
  SetSeg(x = centerX + Round(length * sinAngle),
         y = centerY - Round(length * cosAngle), x1 = centerX - Round(ws + wc),
         y1 = centerY + Round(wc - ws)); /* 1 ---- 2 */
  /* 2 */
  SetSeg(x1, y1, x2 = centerX - Round(ws - wc),
         y2 = centerY + Round(wc + ws)); /* 2 ----- 3 */

  SetSeg(x2, y2, x, y); /* 3 ----- 1(4) */
}

/*
 *  DrawSecond - Draws the second hand (diamond).
 *
 *  length is the maximum length of the hand.
 *  width is the half-width of the hand.
 *  offset is direct distance from Center to tail end.
 *  fractionOfACircle is a fraction between 0 and 1 (inclusive) indicating
 *  how far around the circle (clockwise) from high noon.
 *
 */
void DrawSecond(int length, int width, int offset, int fractionOfACircle) {
  double angle, cosAngle, sinAngle;
  double ms, mc, ws, wc;
  int mid;
  int x, y;

  /*
   *  A full circle is 2 PI radians.
   *  Angles are measured from 12 o'clock, clockwise increasing.
   *  Since in X, +x is to the right and +y is downward:
   *
   *    x = x0 + r * sin(theta)
   *    y = y0 - r * cos(theta)
   *
   */
  angle = TWOPI * fractionOfACircle;
  cosAngle = cos(angle);
  sinAngle = sin(angle);

  /*
   * Order of points when drawing the hand.
   *
   *        1,5
   *        / \
   *       /   \
   *      /     \
   *    2<       >4
   *      \     /
   *       \   /
   *        \ /
   *    -    3
   *    |
   *    |
   *   offset
   *    |
   *    |
   *    -     + center
   */
  mid = (length + offset) / 2;
  mc = mid * cosAngle;
  ms = mid * sinAngle;
  wc = width * cosAngle;
  ws = width * sinAngle;
  /*1 ---- 2 */
  SetSeg(x = centerX + Round(length * sinAngle),
         y = centerY - Round(length * cosAngle), centerX + Round(ms - wc),
         centerY - Round(mc + ws));
  SetSeg(centerX + Round(ms - wc), centerY - Round(mc + ws),
         centerX + Round(offset * sinAngle),
         centerY - Round(offset * cosAngle)); /* 2-----3 */

  SetSeg(centerX + Round(offset * sinAngle),
         centerY - Round(offset * cosAngle), /* 3-----4 */
         centerX + Round(ms + wc), centerY - Round(mc - ws));

  segBufPtr->x = x;
  segBufPtr++->y = y;
  numSegs++;
}

/*
 *  Draw the clock face (every fifth tick-mark is longer
 *  than the others).
 */
void DrawClockFace(void) {
  segBufPtr = segBuf;
  numSegs = 0;
  XFillRectangle(dpy, clockWindow, catGC, 0, 0, DEF_CAT_WIDTH, DEF_CAT_HEIGHT);
}

Pixmap CreateEyePixmap(double t) {
  Pixmap eyeBitmap;
  GC bitmapGC;

  double A = 0.7;
  double omega = 1.0;
  double phi = 3 * M_PI_2;
  double angle;

  double u, w;      /*  Sphere parameters    */
  float r;          /*  Radius               */
  float x0, y0, z0; /*  Center of sphere     */

  XPoint pts[100];

  XGCValues bitmapGCV; /*  GC for drawing       */
  unsigned long valueMask;
  int i, j;

  typedef struct {
    double x, y, z;
  } Point3D;

  /*
   *  Create GC for drawing eyes
   */
  bitmapGCV.function = GXcopy;
  bitmapGCV.plane_mask = AllPlanes;
  bitmapGCV.foreground = 1;
  bitmapGCV.background = 0;
  bitmapGCV.line_width = 15;
  bitmapGCV.line_style = LineSolid;
  bitmapGCV.cap_style = CapRound;
  bitmapGCV.join_style = JoinRound;
  bitmapGCV.fill_style = FillSolid;
  bitmapGCV.subwindow_mode = ClipByChildren;
  bitmapGCV.clip_x_origin = 0;
  bitmapGCV.clip_y_origin = 0;
  bitmapGCV.clip_mask = None;

  valueMask = GCFunction | GCPlaneMask | GCForeground | GCBackground |
              GCLineWidth | GCLineStyle | GCCapStyle | GCJoinStyle |
              GCFillStyle | GCSubwindowMode | GCClipXOrigin | GCClipYOrigin |
              GCClipMask;

  eyeBitmap =
      XCreateBitmapFromData(dpy, root, eyes_bits, eyes_width, eyes_height);
  bitmapGC = XCreateGC(dpy, eyeBitmap, valueMask, &bitmapGCV);

  /*
   *  Compute pendulum function.
   */
  w = M_PI / 2.0;

  angle = A * sin(omega * t + phi) + w;

  x0 = 0.0;
  y0 = 0.0;
  z0 = 2.0;
  r = 1.0;

  for (i = 0, u = -M_PI / 2.0; u < M_PI / 2.0; i++, u += 0.25) {
    Point3D pt;

    pt.x = x0 + r * cos(u) * cos(angle + M_PI / 7.0);
    pt.z = z0 + r * cos(u) * sin(angle + M_PI / 7.0);
    pt.y = y0 + r * sin(u);

    pts[i].x = (int)(((pt.z == 0.0 ? pt.x : pt.x / pt.z) * 23.0) + 12.0);
    pts[i].y = (int)(((pt.z == 0.0 ? pt.y : pt.y / pt.z) * 23.0) + 11.0);
  }

  for (u = M_PI / 2.0; u > -M_PI / 2.0; i++, u -= 0.25) {
    Point3D pt;

    pt.x = x0 + r * cos(u) * cos(angle - M_PI / 7.0);
    pt.z = z0 + r * cos(u) * sin(angle - M_PI / 7.0);
    pt.y = y0 + r * sin(u);

    pts[i].x = (int)(((pt.z == 0.0 ? pt.x : pt.x / pt.z) * 23.0) + 12.0);
    pts[i].y = (int)(((pt.z == 0.0 ? pt.y : pt.y / pt.z) * 23.0) + 11.0);
  }

  /*
   *  Create pixmap for drawing eye (and stippling on update)
   */
  XFillPolygon(dpy, eyeBitmap, bitmapGC, pts, i, Nonconvex, CoordModeOrigin);

  for (j = 0; j < i; j++) {
    pts[j].x += 31;
  }
  XFillPolygon(dpy, eyeBitmap, bitmapGC, pts, i, Nonconvex, CoordModeOrigin);

  XFreeGC(dpy, bitmapGC);

  return (eyeBitmap);
}

void InitializeCat(Pixel catColor, Pixel detailColor, Pixel tieColor) {
  Pixmap catPix;
  Pixmap catBack;
  Pixmap catWhite;
  Pixmap catTie;
  int fillStyle;
  int i;
  XGCValues gcv;
  unsigned long valueMask;
  GC gc1, gc2;

  catPix = XCreatePixmap(dpy, root, DEF_CAT_WIDTH, DEF_CAT_HEIGHT,
                         DefaultDepth(dpy, screen));

  valueMask = GCForeground | GCBackground | GCGraphicsExposures;

  gcv.background = appData.background;
  gcv.foreground = catColor;
  gcv.graphics_exposures = False;
  gcv.line_width = 0;
  gc1 = XCreateGC(dpy, root, valueMask, &gcv);

  fillStyle = FillOpaqueStippled;
  XSetFillStyle(dpy, gc1, fillStyle);

  catBack = XCreateBitmapFromData(dpy, root, catback_bits, catback_width,
                                  catback_height);

  XSetStipple(dpy, gc1, catBack);
  XSetTSOrigin(dpy, gc1, 0, 0);

  XFillRectangle(dpy, catPix, gc1, 0, 0, DEF_CAT_WIDTH, DEF_CAT_HEIGHT);

  fillStyle = FillStippled;
  XSetFillStyle(dpy, gc1, fillStyle);
  XFreeGC(dpy, gc1);

  valueMask = GCForeground | GCBackground | GCGraphicsExposures;

  gcv.background = appData.background;
  gcv.foreground = detailColor;
  gcv.graphics_exposures = False;
  gcv.line_width = 0;
  gc2 = XCreateGC(dpy, root, valueMask, &gcv);

  fillStyle = FillStippled;
  XSetFillStyle(dpy, gc2, fillStyle);
  catWhite = XCreateBitmapFromData(dpy, root, catwhite_bits, catwhite_width,
                                   catwhite_height);

  XSetStipple(dpy, gc2, catWhite);
  XSetTSOrigin(dpy, gc2, 0, 0);
  XFillRectangle(dpy, catPix, gc2, 0, 0, DEF_CAT_WIDTH, DEF_CAT_HEIGHT);
  XFreeGC(dpy, gc2);

  gcv.background = appData.background;
  gcv.foreground = tieColor;
  gcv.graphics_exposures = False;
  gcv.line_width = 0;
  catGC = XCreateGC(dpy, root, valueMask, &gcv);

  fillStyle = FillStippled;
  XSetFillStyle(dpy, catGC, fillStyle);
  catTie = XCreateBitmapFromData(dpy, root, cattie_bits, cattie_width,
                                 cattie_height);

  XSetStipple(dpy, catGC, catTie);
  XSetTSOrigin(dpy, catGC, 0, 0);
  XFillRectangle(dpy, catPix, catGC, 0, 0, DEF_CAT_WIDTH, DEF_CAT_HEIGHT);

  /*
   *  Now, let's create the Backround Pixmap for the Cat Clock using catGC
   *  We will use this pixmap to fill in the window backround.
   */
  fillStyle = FillTiled;
  XSetFillStyle(dpy, catGC, fillStyle);
  XSetTile(dpy, catGC, catPix);
  XSetTSOrigin(dpy, catGC, 0, 0);

  /*
   *  Create the arrays of tail and eye pixmaps
   */
  tailGC = CreateTailGC();
  eyeGC = CreateEyeGC();

  tailPixmap = (Pixmap *)malloc((appData.nTails + 1) * sizeof(Pixmap));
  eyePixmap = (Pixmap *)malloc((appData.nTails + 1) * sizeof(Pixmap));

  for (i = 0; i <= appData.nTails; i++) {

    tailPixmap[i] = CreateTailPixmap(i * M_PI / (appData.nTails));
    eyePixmap[i] = CreateEyePixmap(i * M_PI / (appData.nTails));
  }
}

void UpdateEyesAndTail(void) {
  static int curTail = 0; /*  Index into tail pixmap array       */
  static int tailDir = 1; /*  Left or right swing                */

  /*
   *  Draw new tail & eyes (Don't change values here!!)
   */
  XCopyPlane(dpy, tailPixmap[curTail], clockWindow, tailGC, 0, 0, DEF_CAT_WIDTH,
             TAIL_HEIGHT,
             //               tailGC, 0, 0, DEF_CAT_WIDTH, tail_height,
             0, DEF_CAT_BOTTOM + 1, 0x1);
  XCopyPlane(dpy, eyePixmap[curTail], clockWindow, eyeGC, 0, 0, eyes_width,
             eyes_height, 49, 30, 0x1);

  /*
   *  Figure out which tail & eyes are next
   */

  if (curTail == 0 && tailDir == -1) {
    curTail = 1;
    tailDir = 1;
  } else if (curTail == appData.nTails && tailDir == 1) {
    curTail = appData.nTails - 1;
    tailDir = -1;
  } else {
    curTail += tailDir;
  }
}

void EraseHands(Widget w, struct tm *tm) {

  (void *)w;

  if (numSegs > 0) {
    if (!tm || tm->tm_min != otm.tm_min || tm->tm_hour != otm.tm_hour) {
      XDrawLines(dpy, clockWindow, eraseGC, segBuf, VERTICES_IN_HANDS + 2,
                 CoordModeOrigin);

      XDrawLines(dpy, clockWindow, eraseGC, &(segBuf[VERTICES_IN_HANDS + 2]),
                 VERTICES_IN_HANDS, CoordModeOrigin);

      if (appData.handColor != appData.background) {
        XFillPolygon(dpy, clockWindow, eraseGC, segBuf, VERTICES_IN_HANDS + 2,
                     Convex, CoordModeOrigin);

        XFillPolygon(dpy, clockWindow, eraseGC,
                     &(segBuf[VERTICES_IN_HANDS + 2]), VERTICES_IN_HANDS + 2,
                     Convex, CoordModeOrigin);
      }
    }
  }
}

void Tick(Widget w, int add) {
  static Bool beeped = False; /*  Beeped already?        */
  time_t timeValue;           /*  What time is it?       */
  time(&timeValue);
  tm = *localtime(&timeValue);

  /*
   *  If ticking is to continue, add the next timeout
   */
  if (add) {
    XtAppAddTimeOut(appContext, appData.update, (XtTimerCallbackProc)Tick, w);
  }

  /*
   *  Beep on the half hour; double-beep on the hour.
   */
  if (appData.chime) {
    if (beeped && (tm.tm_min != 30) && (tm.tm_min != 0)) {
      beeped = 0;
    }
    if (((tm.tm_min == 30) || (tm.tm_min == 0)) && (!beeped)) {
      beeped = 1;
      XBell(dpy, 100);
      if (tm.tm_min == 0) {
        XBell(dpy, 100);
      }
    }
  }

  /*
   *  The second (or minute) hand is sec (or min)
   *  sixtieths around the clock face. The hour hand is
   *  (hour + min/60) twelfths of the way around the
   *  clock-face.  The derivation is left as an excercise
   *  for the reader.
   */

  /*
   *  12 hour clock.
   */
  if (tm.tm_hour > 12) {
    tm.tm_hour -= 12;
  }

  if (numSegs == 0 || tm.tm_min != otm.tm_min || tm.tm_hour != otm.tm_hour) {

    segBufPtr = segBuf;
    numSegs = 0;

    DrawClockFace();

    /*
     *  Calculate the minute hand, fill it in with its
     *  color and then outline it.  Next, do the same
     *  with the hour hand.  This is a cheap hidden
     *  line algorithm.
     */
    DrawHand(minuteHandLength, handWidth, ((double)tm.tm_min) / 60.0);
    if (appData.handColor != appData.background) {
      XFillPolygon(dpy, clockWindow, handGC, segBuf, VERTICES_IN_HANDS + 2,
                   Convex, CoordModeOrigin);
    }

    XDrawLines(dpy, clockWindow, highGC, segBuf, VERTICES_IN_HANDS + 2,
               CoordModeOrigin);

    DrawHand(hourHandLength, handWidth,
             ((((double)tm.tm_hour) + (((double)tm.tm_min) / 60.0)) / 12.0));

    if (appData.handColor != appData.background) {
      XFillPolygon(dpy, clockWindow, handGC, &(segBuf[VERTICES_IN_HANDS + 2]),
                   VERTICES_IN_HANDS + 2, Convex, CoordModeOrigin);
    }

    XDrawLines(dpy, clockWindow, highGC, &(segBuf[VERTICES_IN_HANDS + 2]),
               VERTICES_IN_HANDS + 2, CoordModeOrigin);
  }

  UpdateEyesAndTail();

  otm = tm;
  XSync(dpy, False);
}

void HandleExpose(Widget w, XtPointer clientData, XtPointer _callData) {

  (void *)w;
  (void *)clientData;

  XmDrawingAreaCallbackStruct *callData =
      (XmDrawingAreaCallbackStruct *)_callData;

  /*
   *  Ignore if more expose events for this window in the queue
   */
  if (((XExposeEvent *)(callData->event))->count > 0) {
    return;
  }

  /*
   *  Redraw the clock face in the correct mode
   */

  DrawClockFace();
}

void ExitCallback(Widget w, XtPointer clientData, XtPointer callData) {
  (void *)w;
  (void *)clientData;
  (void *)callData;
  exit(0);
}

void HandleInput(Widget w, XtPointer clientData, XtPointer _callData) {

  XmDrawingAreaCallbackStruct *callData =
      (XmDrawingAreaCallbackStruct *)_callData;

  if (callData->event->type != ButtonRelease) {
    return;
  }

  if (((XButtonEvent *)(callData->event))->button == Button2) {
    ExitCallback(w, clientData, _callData);
  }
}

int main(int argc, char **argv) {
  int n;
  Arg args[10];
  Widget topLevel, canvas;
  XGCValues gcv;
  u_long valueMask;

  static XtResource resources[] = {
      {XtNfont, XtCFont, XtRFontStruct, sizeof(XFontStruct *),
       XtOffset(ApplicationDataPtr, font), XtRString,
       (XtPointer)DEF_DIGITAL_FONT},

      {XtNforeground, XtCForeground, XtRPixel, sizeof(Pixel),
       XtOffset(ApplicationDataPtr, foreground), XtRString,
       (XtPointer) "XtdefaultForeground"},

      {XtNbackground, XtCBackground, XtRPixel, sizeof(Pixel),
       XtOffset(ApplicationDataPtr, background), XtRString,
       (XtPointer) "XtdefaultBackground"},

      {"highlight", "HighlightColor", XtRPixel, sizeof(Pixel),
       XtOffset(ApplicationDataPtr, highlightColor), XtRString,
       (XtPointer) "XtdefaultForeground"},

      {"hands", "Hands", XtRPixel, sizeof(Pixel),
       XtOffset(ApplicationDataPtr, handColor), XtRString,
       (XtPointer) "XtdefaultForeground"},

      {"catColor", "CatColor", XtRPixel, sizeof(Pixel),
       XtOffset(ApplicationDataPtr, catColor), XtRString,
       (XtPointer) "XtdefaultForeground"},

      {"detailColor", "DetailColor", XtRPixel, sizeof(Pixel),
       XtOffset(ApplicationDataPtr, detailColor), XtRString,
       (XtPointer) "XtdefaultBackground"},

      {"tieColor", "TieColor", XtRPixel, sizeof(Pixel),
       XtOffset(ApplicationDataPtr, tieColor), XtRString,
       (XtPointer) "XtdefaultBackground"},

      {"padding", "Padding", XtRInt, sizeof(int),
       XtOffset(ApplicationDataPtr, padding), XtRImmediate, (XtPointer)UNINIT},

      {"chime", "Chime", XtRBoolean, sizeof(Boolean),
       XtOffset(ApplicationDataPtr, chime), XtRImmediate, (XtPointer)False},

      {"help", "Help", XtRBoolean, sizeof(Boolean),
       XtOffset(ApplicationDataPtr, help), XtRImmediate, (XtPointer)False},
  };

  argv[0] = "xclock";

  topLevel = XtAppInitialize(&appContext, "Catclock", NULL, 0, &argc, argv,
                             NULL, NULL, 0);

  XtGetApplicationResources(topLevel, &appData, resources, XtNumber(resources),
                            NULL, 0);

  dpy = XtDisplay(topLevel);
  screen = DefaultScreen(dpy);
  root = DefaultRootWindow(dpy);

  /*
   *  "ParseGeometry"  looks at the user-specified geometry
   *  specification string, and attempts to apply it in a rational
   *  fashion to the clock in whatever mode it happens to be in.
   *  For example, for analog mode, any sort of geometry is OK, but
   *  the cat must be a certain size, although the x and y origins
   *  should not be ignored, etc.
   */
  ParseGeometry(topLevel);

  /*
   *  "canvas" is the display widget
   */

  n = 0;
  XtSetArg(args[n], XmNtopAttachment, XmATTACH_FORM);
  n++;
  XtSetArg(args[n], XmNbottomAttachment, XmATTACH_FORM);
  n++;
  XtSetArg(args[n], XmNleftAttachment, XmATTACH_FORM);
  n++;
  XtSetArg(args[n], XmNrightAttachment, XmATTACH_FORM);
  n++;
  XtSetArg(args[n], XmNforeground, appData.foreground);
  n++;
  XtSetArg(args[n], XmNbackground, appData.background);
  n++;
  canvas = XmCreateDrawingArea(topLevel, "drawingArea", args, n);
  XtManageChild(canvas);

  /*
   *  Make all the windows, etc.
   */
  XtRealizeWidget(topLevel);

  /*
   *  Cache the window associated with the XmDrawingArea
   */
  clockWindow = XtWindow(canvas);

  /*
   *  Create the GC's
   */
  valueMask = (GCForeground | GCBackground | GCFont | GCLineWidth |
               GCGraphicsExposures) &
              ~GCFont;

  gcv.background = appData.background;
  gcv.foreground = appData.foreground;

  gcv.graphics_exposures = False;
  gcv.line_width = 0;

  gc = XCreateGC(dpy, clockWindow, valueMask, &gcv);

  valueMask = GCForeground | GCLineWidth;
  gcv.foreground = appData.background;
  eraseGC = XCreateGC(dpy, clockWindow, valueMask, &gcv);

  gcv.foreground = appData.highlightColor;
  highGC = XCreateGC(dpy, clockWindow, valueMask, &gcv);

  valueMask = GCForeground;
  gcv.foreground = appData.handColor;
  handGC = XCreateGC(dpy, clockWindow, valueMask, &gcv);

  appData.nTails = DEF_N_TAILS;
  appData.padding = DEF_ANALOG_PADDING;

  /*
   *  Update rate depends on number of tails,
   *  so tail swings at approximately 60 hz.
   */
  appData.update = (int)(1000.0 / appData.nTails);

  /*
   *  Set the sizes of the hands for analog and cat mode
   */

  radius = Round((min(DEF_CAT_WIDTH, DEF_CAT_HEIGHT) - (2 * appData.padding)) /
                 3.45);

  secondHandLength = ((SECOND_HAND_FRACT * radius) / 100);
  minuteHandLength = ((MINUTE_HAND_FRACT * radius) / 100);
  hourHandLength = ((HOUR_HAND_FRACT * radius) / 100);

  handWidth = ((HAND_WIDTH_FRACT * radius) / 100) * 2;
  secondHandWidth = ((SECOND_WIDTH_FRACT * radius) / 100);

  centerX = DEF_CAT_WIDTH / 2;
  centerY = DEF_CAT_HEIGHT / 2;

  InitializeCat(appData.catColor, appData.detailColor, appData.tieColor);

  {
    XtAddCallback(canvas, XmNexposeCallback, HandleExpose, NULL);
    XtAddCallback(canvas, XmNinputCallback, HandleInput, NULL);
  }

  Tick(canvas, True);
  XtAppMainLoop(appContext);

  return 0;
}
