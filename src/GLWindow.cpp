#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Slider.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Gl_Window.H>
#include <FL/gl.h>
#include <FL/glu.h>
#include "GLWindow.h"
#include "trackball.h"
#include "SocketManager.h"
#include <math.h>


// single set of interaction flags and states
static GLint gDollyPanStartPoint[2] = {0, 0};
static GLfloat gTrackBallRotation [4] = {0.0f, 0.0f, 0.0f, 0.0f};
static GLboolean gDolly = GL_FALSE;
static GLboolean gPan = GL_FALSE;
static GLboolean gTrackball = GL_FALSE;
static GLboolean gTrackingView = GL_FALSE;
static recVec gOrigin = {0.0, 0.0, 0.0};

static bool line_callback(int client_id, const char * line,void *data) {
	return ((GLWindow*)data)->command(client_id,line);
}
static void pause_callback(void * data) {
	//sometimes even after posting a redraw() with a refresh and then
	//pausing the callbacks, the redraw will never be fulfilled
	//this might be an fltk bug or some other race condition
	
	//to get around it, the socket manager will call this pause function
	//which will call redraw, making sure that the screen eventually is redraw
	//and we can re-enable callbacks.
	((GLWindow*)data)->redraw();
}

GLWindow::GLWindow(int X,int Y,int W,int H) : Fl_Gl_Window(X,Y,W,H,NULL) {
    refresh_posted = false;
    clear_posted = false;
    color_by = 0;
    legend_color_by = 0;
    resetCamera();
	shapeSize = 7.0f; // max radius of of objects
	
	point_size = 5.0;
	filter_value = 1.0;
	
	frame = Frame_init();
	SocketManager_init(line_callback,pause_callback,this);
    end();
}

static int readFloats(int n, const char * buf, float * data) {
	for(int i = 0; i < n; i++) {
		char * next;
		data[i] = strtod(buf, &next);
		if(next == buf) {
			printf("warning invalid float input: %s, setting to 0\n",buf);
			return 0;
		}
		buf = next;
	}
	return 1;
}

void GLWindow::updateProjection() {
	GLdouble ratio, radians, wd2;
	GLdouble left, right, top, bottom, near_, far_;

    
	// set projection
	glMatrixMode (GL_PROJECTION);
	glLoadIdentity ();
	//near = -camera.viewPos.z - shapeSize * 0.5;
	near_ = 0.001;
	far_ = 100; //-camera.viewPos.z + shapeSize * 0.5;
	if (far_ < 1.0)
		far_ = 1.0;
	
	radians = 0.0174532925 * camera.aperture / 2; // half aperture degrees to radians 
	wd2 = near_ * tan(radians);
	ratio = camera.viewWidth / (float) camera.viewHeight;
	if (ratio >= 1.0) {
		left  = -ratio * wd2;
		right = ratio * wd2;
		top = wd2;
		bottom = -wd2;	
	} else {
		left  = -wd2;
		right = wd2;
		top = wd2 / ratio;
		bottom = -wd2 / ratio;	
	}
	glFrustum (left, right, bottom, top, near_, far_);
}

void GLWindow::updateModelView() {
	
	// move view
	glMatrixMode (GL_MODELVIEW);
	glLoadIdentity ();
	gluLookAt (camera.viewPos.x, camera.viewPos.y, camera.viewPos.z,
			   camera.viewPos.x + camera.viewDir.x,
			   camera.viewPos.y + camera.viewDir.y,
			   camera.viewPos.z + camera.viewDir.z,
			   camera.viewUp.x, camera.viewUp.y ,camera.viewUp.z);
			
	// if we have trackball rotation to map (this IS the test I want as it can be explicitly 0.0f)
	if (gTrackingView && gTrackBallRotation[0] != 0.0f) 
		glRotatef (gTrackBallRotation[0], gTrackBallRotation[1], gTrackBallRotation[2], gTrackBallRotation[3]);
	else {
	}
	// accumlated world rotation via trackball
	glRotatef (worldRotation[0], worldRotation[1], worldRotation[2], worldRotation[3]);
}

void GLWindow::check_resize(int width, int height) {
	// ensure camera knows size changed
	if ((camera.viewHeight != height) ||
	    (camera.viewWidth != width)) {
		camera.viewHeight = height;
		camera.viewWidth = width;
		
		glViewport (0, 0, camera.viewWidth, camera.viewHeight);
		updateProjection();
	}
}
void GLWindow::resize(int X,int Y,int W,int H) {
    Fl_Gl_Window::resize(X,Y,W,H);
    check_resize(W,H);
}

void GLWindow::mouseDolly(int x, int y) {
	GLfloat dolly = (gDollyPanStartPoint[1] - y) * -camera.viewPos.z / 300.0f;
	camera.viewPos.z += dolly;
	if (camera.viewPos.z == 0.0) // do not let z = 0.0
		camera.viewPos.z = 0.0001;
	gDollyPanStartPoint[0] = x;
	gDollyPanStartPoint[1] = y;
}

void GLWindow::mousePan(int x, int y) {
	GLfloat panX = (gDollyPanStartPoint[0] - x) / (900.0f / -camera.viewPos.z);
	GLfloat panY = (gDollyPanStartPoint[1] - y) / (900.0f / -camera.viewPos.z);
	camera.viewPos.x -= panX;
	camera.viewPos.y -= panY;
	gDollyPanStartPoint[0] = x;
	gDollyPanStartPoint[1] = y;
}

void GLWindow::resetCamera() {
   camera.aperture = 40;
   camera.rotPoint = gOrigin;

   camera.viewPos.x = 0.0;
   camera.viewPos.y = 0.0;
   camera.viewPos.z = -10.0;
   camera.viewDir.x = 0; 
   camera.viewDir.y = 0; 
   camera.viewDir.z = 1;

   camera.viewUp.x = 0;  
   camera.viewUp.y = 1; 
   camera.viewUp.z = 0;
   
   memset(worldRotation,0,sizeof(GLfloat) * 4);
   memset(objectRotation,0,sizeof(GLfloat) * 4);
   scroll_delta[0] = scroll_delta[1] = 0;
}
void GLWindow::interactive_clear() {
    clear(true);
    refresh_posted = 1;
	redraw();
}
void GLWindow::clear(bool reset_bb) {
	Frame_clear(frame,reset_bb);
	if(reset_bb) {
		for(int i = 0; i < LABEL_SIZE; i++) {
			label_table[i].clear();
		}
		refresh_legend();
	}
}

void GLWindow::mouseDown(int x, int y) {
	gDolly = GL_FALSE; // no dolly
	gPan = GL_FALSE; // no pan
	gTrackball = GL_TRUE;
	startTrackball (x, y, 0, 0, camera.viewWidth, camera.viewHeight);
	gTrackingView = GL_TRUE;
	scroll_delta[0] = scroll_delta[1] = 0;
}

void GLWindow::rightMouseDown(int x, int y) {
	if (gTrackball) { // if we are currently tracking, end trackball
		if (gTrackBallRotation[0] != 0.0)
			addToRotationTrackball (gTrackBallRotation, worldRotation);
		gTrackBallRotation [0] = gTrackBallRotation [1] = gTrackBallRotation [2] = gTrackBallRotation [3] = 0.0f;
	}
	gDolly = GL_FALSE; // no dolly
	gPan = GL_TRUE; 
	gTrackball = GL_FALSE; // no trackball
	gDollyPanStartPoint[0] = x;
	gDollyPanStartPoint[1] = y;
	gTrackingView = GL_TRUE;
	scroll_delta[0] = scroll_delta[1] = 0;
}

void GLWindow::otherMouseDown(int x, int y) {
	if (gTrackball) { // if we are currently tracking, end trackball
		if (gTrackBallRotation[0] != 0.0)
			addToRotationTrackball (gTrackBallRotation, worldRotation);
		gTrackBallRotation [0] = gTrackBallRotation [1] = gTrackBallRotation [2] = gTrackBallRotation [3] = 0.0f;
	}
	gDolly = GL_TRUE;
	gPan = GL_FALSE; // no pan
	gTrackball = GL_FALSE; // no trackball
	gDollyPanStartPoint[0] = x;
	gDollyPanStartPoint[1] = y;
	gTrackingView = GL_TRUE;
	scroll_delta[0] = scroll_delta[1] = 0;
}

void GLWindow::mouseUp(int x, int y) {
	if (gDolly) { // end dolly
		gDolly = GL_FALSE;
	} else if (gPan) { // end pan
		gPan = GL_FALSE;
	} else if (gTrackball) { // end trackball
		gTrackball = GL_FALSE;
		if (gTrackBallRotation[0] != 0.0)
			addToRotationTrackball (gTrackBallRotation, worldRotation);
		gTrackBallRotation [0] = gTrackBallRotation [1] = gTrackBallRotation [2] = gTrackBallRotation [3] = 0.0f;
	} 
	gTrackingView = GL_FALSE;
}

void GLWindow::mouseDragged(int x, int y) {
	if (gTrackball) {
		rollToTrackball (x, y, gTrackBallRotation);
		redraw();
	} else if (gDolly) {
		mouseDolly(x,y);
		updateProjection();
		redraw();
	} else if (gPan) {
		mousePan(x,y);
		redraw();
	}
}

void GLWindow::scrollWheel(int x, int y, int delta_x, int delta_y) {
#if 0
	scroll_delta[0] += delta_x;
	scroll_delta[1] += delta_y;
	
	if(gTrackball || gDolly || gPan) {
		mouseDragged(x + scroll_delta[0],y + scroll_delta[1]);
	} else 
#endif
	{
		float wheelDelta = delta_x - delta_y;
		if (wheelDelta) {
			GLfloat deltaZ = wheelDelta * -camera.aperture / 200.0f;
			camera.viewPos.z -= deltaZ;
			if(camera.viewPos.z == 0.f)
				camera.viewPos.z = .001f;
			updateProjection();
			redraw();
		}
	}
}

int GLWindow::handle(int event) {
	int x = Fl::event_x();
	int y = Fl::event_y();
	
	int dx = Fl::event_dx();
	int dy = Fl::event_dy();
	
	
	switch(event) {
		case FL_PUSH:
			switch(Fl::event_button()) {
				case FL_LEFT_MOUSE:
					mouseDown(x,y);
					break;
				case FL_RIGHT_MOUSE:
					rightMouseDown(x,y);
					break;
			}
			break;
		case FL_DRAG:
			mouseDragged(x,y);
			break;
		case FL_RELEASE:
			mouseUp(x,y);
			break;
		case FL_MOUSEWHEEL:
			scrollWheel(x,y,dx,dy);
			break;
		default:
			return Fl_Gl_Window::handle(event);
			break;
	}
	return 1;
}

void GLWindow::prepareOpenGL(int width, int height) {
	glEnable(GL_DEPTH_TEST);

	glShadeModel(GL_SMOOTH);    
	glFrontFace(GL_CCW);
	glPolygonOffset (1.0f, 1.0f);
	
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
}

bool GLWindow::command(int client_id,const char * buf) {
	//printf("%s\n",buf);
	float data[9];
	ClientState & state = client_state[client_id];
	Frame_setColor(frame,state.colors);
	
	switch(buf[0]) {
		case 'p':
			if(readFloats(3, buf+1, data)) {
				Frame_addPoint(frame, data);
			}
			break;
		case 't':
			if(readFloats(9, buf+1, data)) {
				Frame_addTriangle(frame, data);
			}
			break;
		case 'c':
			if(readFloats(3, buf+1, data)) {
				memcpy(&state.colors[0],data,sizeof(Color));
			}
			break;
		case 'l':
			if(readFloats(6, buf+1, data)) {
				Frame_addLine(frame, data);
			}
			break;
		case 'n':
			if(readFloats(6, buf+1, data)) {
				Frame_addNormal(frame, data);
			}
			break;
		case 's': {
			char * start_lbl;
			int key = strtol(buf+1,&start_lbl,10);
			if(start_lbl != buf+1 && *start_lbl != '\0') {
				start_lbl++;
				int string = string_table.Intern(start_lbl);
				state.client_key_to_string[key] = string;
			} else {
				printf("warning malformed lbl\n");
			}
		} break;
		case 'g': {
			if(readFloats(2,buf+1,data)) {
				unsigned int label = data[0];
				unsigned int key = data[1];
				if(label < LABEL_SIZE && state.client_key_to_string.count(key)) {
					int string = state.client_key_to_string[key];
					label_table[label].colorFor(string,&state.colors[label+1]);
				}
			}
		} break;
		case 'f':
			if(refresh_posted) {
				//it is unsafe to clear the frame now because we need to redraw it first,
				//we note that we need to clear the screen after the redraw
				//and disable all new commands until it is done
				clear_posted = true;
				//printf("pausing callbacks waiting for refresh\n");
				return false;
			} else {
				clear(false);
			}
			break;
		case 'r':
			refresh_posted = true;
			redraw();
			break;
		default:
			printf("ignoring unknown command: %c\n",buf[0]);
			break;
	}
	return true;
}
void GLWindow::refresh_legend() {
	if(legend_color_by != color_by || (color_by > 0 && legend->size() != label_table[color_by-1].names.size())) {
		if(color_by > 0) {
			
			LabelTable & l = label_table[color_by - 1];
			
			if(l.names.size() > 0)
				legend->show();
			else
				legend->hide();
				
			legend->clear();
			
			for(int i = 0; i < l.names.size(); i++) {
				char buf[1024];
				snprintf(buf,1024,"@C%d\xE2\x96\x88\xE2\x96\x88\t@C0@.%s\n",i+8,string_table.Extern(l.names[i]));
				legend->add("\t");
				legend->add(buf);
			}
			legend->redraw();
		} else {
			legend->hide();
			legend->clear();
			legend->redraw();
		}
		legend_color_by = color_by;
	}
}
void GLWindow::set_color_by(int c) {
	color_by = c;
	refresh_legend();
	redraw();
}
void GLWindow::draw() {
	if (!valid()) { valid(1); prepareOpenGL(w(), h()); }      // first time? init

	// setup viewport and prespective
	check_resize(w(),h());
	updateModelView();
	
	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	Frame_setVisibleRange(frame, 0, Frame_nObjects(frame) * filter_value);
	refresh_legend();
	if(refresh_posted) {
		Frame_getBBox(frame,&current_bounds);
		Frame_refresh(frame,&current_bounds);
		refresh_posted = false;
	}

	float diag = BBox_diagonal_length(&current_bounds);
	if(diag == 0.f)
		diag = 1.f;
	float center[3];
	BBox_center(&current_bounds, center);
	float scale = 1.f/diag * 5.f;
	glScalef(scale, scale, scale);
	glTranslatef(-center[0],-center[1],-center[2]);
	Frame_draw(frame, point_size, color_by);
	
	GLenum err = glGetError();
	if(GL_NO_ERROR != err) {
		fprintf(stderr,"gl: %s\n", gluErrorString(err));
	}
	
	if(clear_posted) {
		//printf("delayed clear handled, reenableing callbacks\n");
		clear(false);
		clear_posted = false;
		//reenable (and flush) the pending events
		SocketManager_reenableCallbacks();
	}
}