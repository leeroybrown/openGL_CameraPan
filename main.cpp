/*
main.cpp

	Lee Sichello

	This OpenGL program does the following: 

	Upon run it will draw a series of geometrical object (4 blocks , 1 sphere and a floor) using solely triangles. 
	The camera wil be stationary until the user presses '1' or '2'. 
		'1' -	Camera pans from L0 to L1 on a linear path 
		'2' -	Camera rotates about the center of the scene in a spiral pttern from S0 to S1

*/
#include <math.h>
#include "Angel.h"
#ifdef __APPLE__
#ifndef GLUT_3_2_CORE_PROFILE
#define GLUT_3_2_CORE_PROFILE 0
#define glBindVertexArray		glBindVertexArrayAPPLE
#define glDeleteVertexArrays	glDeleteVertexArraysAPPLE
#define glIsVertexArray			glIsVertexArrayAPPLE
#define glGenVertexArrays  	glGenVertexArraysAPPLE
#endif
#endif

#include <cassert>
const float PI = 3.14159;

// Dr. Angel's matrix stack - used to simulate classic OpenGL push and pop
class MatrixStack {
    int _index, _size;
    mat4* _matrices;
public:
	MatrixStack (int numMatrices = 32):_index(0), _size(numMatrices)
    {_matrices = new mat4[numMatrices];}
	
    ~MatrixStack ()
    {delete[] _matrices;}
	
    void push(const mat4 &m)
    {assert (_index + 1 < _size);
		_matrices[_index++] = m;}
	
    mat4& pop(void)
    {assert (_index - 1 >= 0);
		return _matrices[--_index];}
};

//global modelview matrix stack
MatrixStack matStack;

//Define some geometry and colours here
typedef Angel::vec4 point4;
typedef Angel::vec4 color4;

//(6 faces)(2 triangles/face)(3 vertices/triangle)
const int NumVerticesPerBlock = 36; 
point4 points[NumVerticesPerBlock];


// Vertices of a unit cube centered at origin, sides aligned with axes
point4 vertices[8] = {
	point4( -0.5, -0.5, 0.5, 1.0 ),
	point4( -0.5, 0.5, 0.5, 1.0 ),
	point4( 0.5, 0.5, 0.5, 1.0 ),
	point4( 0.5, -0.5, 0.5, 1.0 ),
	point4( -0.5, -0.5, -0.5, 1.0 ),
	point4( -0.5, 0.5, -0.5, 1.0 ),
	point4( 0.5, 0.5, -0.5, 1.0 ),
	point4( 0.5, -0.5, -0.5, 1.0 )
};

// RGBA olors
GLfloat black[4] = { 0.0, 0.0, 0.0, 1.0 }; // black
GLfloat red[4] = { 1.0, 0.0, 0.0, 1.0 }; // red
GLfloat yellow[4] = { 1.0, 1.0, 0.0, 1.0 }; // yellow
GLfloat green[4] = { 0.0, 1.0, 0.0, 1.0 }; // green
GLfloat blue[4] = { 0.0, 0.0, 1.0, 1.0 }; // blue
GLfloat cyan[4] = { 0.0, 1.0, 1.0, 1.0 }; // cyan
GLfloat white[4] = { 1.0, 1.0, 1.0, 1.0 }; // white

//---------------------------------------------------------------------
// quad generates two triangles for each face of a block and adds them to the points array
// integer inputs are the indices for the vertices of the block face under consideration
int Index = 0;
void quad( int a, int b, int c, int d )
{
	points[Index] = vertices[a]; Index++;
	points[Index] = vertices[b]; Index++;
	points[Index] = vertices[c]; Index++;
	points[Index] = vertices[a]; Index++;
	points[Index] = vertices[c]; Index++;
	points[Index] = vertices[d]; Index++;
}
//------------------------------------------------------------------------

// generate 12 triangles: 36 vertices and 36 colors
void buildBlock()
{
	quad( 1, 0, 3, 2 );
	quad( 2, 3, 7, 6 );
	quad( 3, 0, 4, 7 );
	quad( 6, 5, 1, 2 );
	quad( 4, 5, 6, 7 );
	quad( 5, 4, 0, 1 );
}
//------------------------------------------------------------------------

// Classic OpenGL Transformation Matrices
mat4 p;	//projection matrix
mat4 mv;//modelview matrix

//Shader Uniform Index Variables
GLint projIndex;
GLint mvIndex;

GLuint program;
GLint uColor;     //Shader color input
GLint uLightPosition;//Shader light position input

//state of camera movement
const int stopped = 0;
const int linear = 1;
const int spiral = 2;
int camState = stopped;
bool reset = false;		//used to reset cam to starting position

//initial and final positions on linear path 
point4 L0(-6.0,8.0,6.0, 1.0);
point4 L1(6.0,8.0,6.0, 1.0);

//initial and final positions on spiral path
point4 S0(0.0, 5.0, 10.0, 1.0);
point4 S1(0.0, 0.0, 10.0, 1.0);
float theta;		// rotation of camera about Y-axis
point4 spiralCenter(0.0, 0.75, 0.0, 1.0);
float spiralRadius = 10.0f;

//camera positioning
point4 eye = L0;
point4 at(0.0f, 0.75f, 0.0f, 1.0f);
vec4   up(0.0f, 1.0f, 0.0f, 1.0f);

void init()
{  
	buildBlock();

   //Set up shader
   program = InitShader( "vshaderL3.glsl", "fshaderL3.glsl" );

   glUseProgram( program );

   //Get locations of transformation matrices from shader
   mvIndex = glGetUniformLocation(program, "mv");
   projIndex = glGetUniformLocation(program, "p");

   //Get locations of lighting uniforms from shader
   //uLightPosition = glGetUniformLocation(program, "lightPosition");
   uColor = glGetUniformLocation(program, "uColor");

   //Set default lighting and material properties in shader.
   glUniform4f(uLightPosition, 10.0f, 10.0f, 10.0f, 0.0f);
   glUniform4fv(uColor, 1, black);

	glEnable(GL_DEPTH_TEST);
	glClearColor( 1.0, 1.0, 1.0, 1.0 );
}
//----------------------------------------------------------------------------

//dawSolidSphere takes a float input for radius, as well as two integers (slices and stacks) which will define
//the size of the triangles used to represent the smooth sphere. 
//
void drawSolidSphere(GLfloat radius, GLint slices, GLint stacks)
{
// Trig utility definitions
#define toSin(a) sinf((a)/360.0f*2.0f*3.141596f)
#define toCos(a) cosf((a)/360.0f*2.0f*3.141596f)
#define toPhi(_s) (_s)*phiStep  
#define toRho(_t) (_t)*rhoStep


   static GLint lastSlices = 0;
   static GLint lastStacks = 0;
   static GLfloat *vertsArray = NULL;
   static GLfloat *normsArray = NULL;
   static GLfloat *texCoordArray = NULL;
   static GLuint sphere = 0, buffer = 0;

   GLint sphereVerts = 6 * (slices * stacks -  slices);

   //Generate a new sphere ONLY if necessary - not the same dimesions as last time
   if (lastSlices != slices && lastStacks != stacks)
   {
      lastSlices = slices;
      lastStacks = stacks;
      GLfloat phiStep = 360.0f/slices;
      GLfloat rhoStep = 180.0f/stacks;

      //allocate new memory
      vertsArray = new GLfloat[sphereVerts*4];
      normsArray = new GLfloat[sphereVerts*4];

      int i = 0;
      int j = 0;

      //Body of sphere
      for (int s = 0; s < slices; s++)
      {
         for (int t = 1; t < stacks - 1; t++)
         {
            //Triangle 1:
            // v1 *--* v3
            //    | /
            //    |/
            // v2 *  * v4

            //v1
            normsArray[i] = toSin(toPhi(s)) * toSin(toRho(t));
            vertsArray[i++] = normsArray[i] * radius;
            normsArray[i]  = toCos(toPhi(s)) * toSin(toRho(t));
            vertsArray[i++] = normsArray[i] * radius;
            normsArray[i]  = toCos(toRho(t));
            vertsArray[i++] = normsArray[i] * radius;
            normsArray[i] = 0.0f;
            vertsArray[i++] = 1.0f;

            //v2
            normsArray[i] = toSin(toPhi(s)) * toSin(toRho(t+1));
            vertsArray[i++] = normsArray[i] * radius;
            normsArray[i] = toCos(toPhi(s)) * toSin(toRho(t+1));
            vertsArray[i++] = normsArray[i] * radius;
            normsArray[i] = toCos(toRho(t+1));
            vertsArray[i++] = normsArray[i] * radius;
            normsArray[i] = 0.0f;
            vertsArray[i++] = 1.0f;

            //v3
            normsArray[i] = toSin(toPhi(s+1)) * toSin(toRho(t));
            vertsArray[i++] = normsArray[i] * radius;
            normsArray[i] = toCos(toPhi(s+1)) * toSin(toRho(t));
            vertsArray[i++] = normsArray[i] * radius;
            normsArray[i] = toCos(toRho(t));
            vertsArray[i++] = normsArray[i] * radius;
            normsArray[i] = 0.0f;
            vertsArray[i++] = 1.0f;

            //Triangle 2:
            // v1 *  * v3
            //      /|
            //     / |
            // v2 *--* v4

            //v3
            normsArray[i] = toSin(toPhi(s+1)) * toSin(toRho(t));
            vertsArray[i++] = normsArray[i] * radius;
            normsArray[i] = toCos(toPhi(s+1)) * toSin(toRho(t));
            vertsArray[i++] = normsArray[i] * radius;
            normsArray[i] = toCos(toRho(t));
            vertsArray[i++] = normsArray[i] * radius;
            normsArray[i] = 0.0f;
            vertsArray[i++] = 1.0f;

            //v2
            normsArray[i] = toSin(toPhi(s)) * toSin(toRho(t+1));
            vertsArray[i++] = normsArray[i] * radius;
            normsArray[i] = toCos(toPhi(s)) * toSin(toRho(t+1));
            vertsArray[i++] = normsArray[i] * radius;
            normsArray[i] = toCos(toRho(t+1));
            vertsArray[i++] = normsArray[i] * radius;
            normsArray[i] = 0.0f;
            vertsArray[i++] = 1.0f;

            //v4
            normsArray[i] = toSin(toPhi(s+1)) * toSin(toRho(t+1));
            vertsArray[i++] = normsArray[i] * radius;
            normsArray[i] = toCos(toPhi(s+1)) * toSin(toRho(t+1));
            vertsArray[i++] = normsArray[i] * radius;
            normsArray[i] = toCos(toRho(t+1));
            vertsArray[i++] = normsArray[i] * radius;
            normsArray[i] = 0.0f;
            vertsArray[i++] = 1.0f;
         }
      }
      //Top (Special because v2 and v3 are always both 0,0)
      for (int s = 0; s < slices; s++)
      {
         int t = 0;
         //Triangle:
         // v1 * (v3)
         //    |\  
         //    | \ 
         // v2 *--* v4

         //v1
         normsArray[i] = toSin(toPhi(s)) * toSin(toRho(t));
         vertsArray[i++] = normsArray[i] * radius;
         normsArray[i] = toCos(toPhi(s)) * toSin(toRho(t));
         vertsArray[i++] = normsArray[i] * radius;
         normsArray[i] = toCos(toRho(t));
         vertsArray[i++] = normsArray[i] * radius;
         normsArray[i] = 0.0f;
         vertsArray[i++] = 1.0f;

         //v2
         normsArray[i] = toSin(toPhi(s)) * toSin(toRho(t+1));
         vertsArray[i++] = normsArray[i] * radius;
         normsArray[i] = toCos(toPhi(s)) * toSin(toRho(t+1));
         vertsArray[i++] = normsArray[i] * radius;
         normsArray[i] = toCos(toRho(t+1));
         vertsArray[i++] = normsArray[i] * radius;
         normsArray[i] = 0.0f;
         vertsArray[i++] = 1.0f;

         //v4
         normsArray[i] = toSin(toPhi(s+1)) * toSin(toRho(t+1));
         vertsArray[i++] = normsArray[i] * radius;
         normsArray[i] = toCos(toPhi(s+1)) * toSin(toRho(t+1));
         vertsArray[i++] = normsArray[i] * radius;
         normsArray[i] = toCos(toRho(t+1));
         vertsArray[i++] = normsArray[i] * radius;
         normsArray[i] = 0.0f;
         vertsArray[i++] = 1.0f;
      }


      //Bottom (Special because v2 and v4 are always both 180,180)
      for (int s = 0; s < slices; s++)
      {
         int t = stacks-1;
         //Triangle:
         // v1 *--* v3
         //    | /
         //    |/
         // v2 *  * v4

         //v1
         normsArray[i] = toSin(toPhi(s)) * toSin(toRho(t));
         vertsArray[i++] = normsArray[i] * radius;
         normsArray[i] = toCos(toPhi(s)) * toSin(toRho(t));
         vertsArray[i++] = normsArray[i] * radius;
         normsArray[i] =   toCos(toRho(t));
         vertsArray[i++] = normsArray[i] * radius;
         normsArray[i] = 0.0f;
         vertsArray[i++] = 1.0f;

         //v2
         normsArray[i] = toSin(toPhi(s)) * toSin(toRho(t+1));
         vertsArray[i++] = normsArray[i] * radius;
         normsArray[i] = toCos(toPhi(s)) * toSin(toRho(t+1));
         vertsArray[i++] = normsArray[i] * radius;
         normsArray[i] = toCos(toRho(t+1));
         vertsArray[i++] = normsArray[i] * radius;
         normsArray[i] = 0.0f;
         vertsArray[i++] = 1.0f;

         //v3
         normsArray[i] = toSin(toPhi(s+1)) * toSin(toRho(t));
         vertsArray[i++] = normsArray[i] * radius;
         normsArray[i] = toCos(toPhi(s+1)) * toSin(toRho(t));
         vertsArray[i++] = normsArray[i] * radius;
         normsArray[i] = toCos(toRho(t));
         vertsArray[i++] = normsArray[i] * radius;
         normsArray[i] = 0.0f;
         vertsArray[i++] = 1.0f;
      }

      if (!glIsVertexArray(sphere))
      {
         glGenVertexArrays( 1, &sphere );
      }

      glBindVertexArray( sphere );

      if (glIsBuffer(buffer))
      {
         glDeleteBuffers(1, &buffer);
      }
      glGenBuffers( 1, &buffer );
      glBindBuffer( GL_ARRAY_BUFFER, buffer );
      glBufferData( GL_ARRAY_BUFFER, sphereVerts*4*sizeof(GLfloat)*2, NULL, GL_STATIC_DRAW );
      glBufferSubData( GL_ARRAY_BUFFER, 0, sphereVerts*4*sizeof(GLfloat), vertsArray );
      glBufferSubData( GL_ARRAY_BUFFER, sphereVerts*4*sizeof(GLfloat), sphereVerts*4*sizeof(GLfloat), normsArray );
      GLuint vPosition = glGetAttribLocation( program, "vPosition" );
      glEnableVertexAttribArray( vPosition );
      glVertexAttribPointer( vPosition, 4, GL_FLOAT, GL_FALSE, 0,BUFFER_OFFSET(0) );
      GLuint vNormal = glGetAttribLocation( program, "vNormal" ); 
      glEnableVertexAttribArray( vNormal );
      glVertexAttribPointer( vNormal, 4, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(sphereVerts*4*sizeof(GLfloat)) );

      delete [] vertsArray;
      delete [] normsArray;
      delete [] texCoordArray;

   }

   glBindVertexArray(sphere);
   glDrawArrays(GL_TRIANGLES, 0, sphereVerts);

}
//----------------------------------------------------------------------------
void drawSolidCube()
{
	const vec4 cubeVerts[] = 
	{
		vec4( 0.5, 0.5, 0.5, 1), //0
		vec4( 0.5, 0.5,-0.5, 1), //1
		vec4( 0.5,-0.5, 0.5, 1), //2
		vec4( 0.5,-0.5,-0.5, 1), //3
		vec4(-0.5, 0.5, 0.5, 1), //4
		vec4(-0.5, 0.5,-0.5, 1), //5
		vec4(-0.5,-0.5, 0.5, 1), //6
		vec4(-0.5,-0.5,-0.5, 1)  //7
	};

	const vec4 verts[] = //36 vertices total
	{
		cubeVerts[0], cubeVerts[4], cubeVerts[6],  //front
		cubeVerts[0], cubeVerts[6], cubeVerts[2],  
		cubeVerts[1], cubeVerts[0], cubeVerts[2],  //right 
		cubeVerts[1], cubeVerts[2], cubeVerts[3],
		cubeVerts[5], cubeVerts[1], cubeVerts[3],  //back  
		cubeVerts[5], cubeVerts[3], cubeVerts[7],
		cubeVerts[4], cubeVerts[5], cubeVerts[7],  //left  
		cubeVerts[4], cubeVerts[7], cubeVerts[6],
		cubeVerts[4], cubeVerts[0], cubeVerts[1],  //top 
		cubeVerts[4], cubeVerts[1], cubeVerts[5],
		cubeVerts[6], cubeVerts[7], cubeVerts[3],  //bottom 
		cubeVerts[6], cubeVerts[3], cubeVerts[2],

	}; 

	const vec4 right =   vec4( 1.0f, 0.0f, 0.0f, 0.0f);
	const vec4 left = vec4(-1.0f, 0.0f, 0.0f, 0.0f);
	const vec4 top =  vec4( 0.0f, 1.0f, 0.0f, 0.0f);
	const vec4 bottom =  vec4( 0.0f,-1.0f, 0.0f, 0.0f);
	const vec4 front =   vec4( 0.0f, 0.0f, 1.0f, 0.0f);
	const vec4 back = vec4( 0.0f, 0.0f,-1.0f, 0.0f);

	const vec4 normsArray[] = 
	{
		front, front, front, front, front, front,
		right, right, right, right, right, right,
		back, back, back, back, back, back,
		left, left, left, left, left, left,
		top, top, top, top, top, top,
		bottom, bottom, bottom, bottom, bottom, bottom

	};

	static GLuint cube = 0, buffer = 0;
   
	vec4 vertsArray[36];

	for(int i = 0; i < 36; i++)
	{
		vertsArray[i] = verts[i];
		vertsArray[i].w = 1.0f;
	}

	if (!glIsVertexArray(cube))
	{
		glGenVertexArrays( 1, &cube );
	}

	glBindVertexArray( cube );

	if (glIsBuffer(buffer))
	{
		glDeleteBuffers(1, &buffer);
	}
	glGenBuffers( 1, &buffer );
	glBindBuffer( GL_ARRAY_BUFFER, buffer );
	glBufferData( GL_ARRAY_BUFFER, 36*4*sizeof(GLfloat)*2, NULL, GL_STATIC_DRAW );
	glBufferSubData( GL_ARRAY_BUFFER, 0, 36*4*sizeof(GLfloat), vertsArray );
	glBufferSubData( GL_ARRAY_BUFFER, 36*4*sizeof(GLfloat), 36*4*sizeof(GLfloat), normsArray );
	GLuint vPosition = glGetAttribLocation( program, "vPosition" );
	glEnableVertexAttribArray( vPosition );
	glVertexAttribPointer( vPosition, 4, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(0) );
	GLuint vNormal = glGetAttribLocation( program, "vNormal" ); 
	glEnableVertexAttribArray( vNormal );
	glVertexAttribPointer( vNormal, 4, GL_FLOAT, GL_FALSE, 0, BUFFER_OFFSET(36*4*sizeof(GLfloat)) );
   

   glBindVertexArray(cube);
   glDrawArrays(GL_TRIANGLES, 0, 36);

}
//----------------------------------------------------------------------------

void display( void )
{
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	//draw floor
	matStack.push(mv);
	mv *= Scale(10.0, 0.01f, 10.0f);
	glUniform4f(uColor, 0.9, 0.9, 0.9, 0.9);
	glUniformMatrix4fv(mvIndex, 1, GL_TRUE, mv);
	drawSolidCube();
	mv = matStack.pop();

	//draw sphere
	matStack.push(mv);
	mv *= Translate(0.0f, 1.0f, 0.0f);
	glUniformMatrix4fv(mvIndex, 1, GL_TRUE, mv);
	glUniform4fv(uColor, 1, blue);
	drawSolidSphere(1.0f,20.0,10);
	mv = matStack.pop();

	//draw B1
	matStack.push(mv);
	mv *= Translate(1.5f, 1.5f, 0.0f);
	mv *= Scale(1.0f, 3.0f, 1.0f);
	glUniform4fv(uColor, 1, cyan);
	glUniformMatrix4fv(mvIndex, 1, GL_TRUE, mv);
	drawSolidCube();
	mv = matStack.pop();	

	//draw B2
	matStack.push(mv);
	mv *= Translate(-1.5f, 0.75f, 0.0f);
	mv *= Scale(1.0f, 1.5f, 1.0f);	
	glUniform4fv(uColor, 1, red);
	glUniformMatrix4fv(mvIndex, 1, GL_TRUE, mv);
	drawSolidCube();
	mv = matStack.pop();

	//draw B3
	matStack.push(mv);
	mv *= Translate(0.0f, 0.5f, 1.5f);
	mv *= Scale(2.0f, 1.0f, 1.0f);	
	glUniform4fv(uColor, 1, yellow);
	glUniformMatrix4fv(mvIndex, 1, GL_TRUE, mv);
	drawSolidCube();
	mv = matStack.pop();

	//draw B4
	matStack.push(mv);
	mv *= Translate(0.0f, 0.5f, -1.5f);
	mv *= Scale(2.0f, 1.0f, 1.0f);	
	glUniform4fv(uColor, 1, green);
	glUniformMatrix4fv(mvIndex, 1, GL_TRUE, mv);
	drawSolidCube();
	mv = matStack.pop();

	mv = mat4();
	mv = LookAt(eye, at, up);

	//Show what was drawn
	glutSwapBuffers();
}
//----------------------------------------------------------------------------

void reshape (int w, int h)
{
	glViewport(0, 0, (GLsizei) w, (GLsizei) h);
	p = mat4();
	p *= Ortho(-4,4,-4,4,1,20);
	glUniformMatrix4fv(projIndex, 1, GL_TRUE, p);
}

//----------------------------------------------------------------------------

void keyboard (unsigned char key, int x, int y)
{
	switch (key) {
	case '1':
		reset = true;
		camState = linear;
		break;
	case '2':
		reset = true;
		camState = spiral;
		break;
	case 27:
	case 'q':
	case 'Q':
		exit(0);
		break;
	default:
		break;
	}
}
//----------------------------------------------------------------------------
void idle(){
	int spiralTimescale = 10000;
	if(camState == stopped){
		return;
	}
	else if(camState == linear){
		if (reset){
			reset = false;
			eye = L0;
		}
		else if( length(eye-L0) < length(L1-L0) ){
			eye.x += length(L1-L0)/5000;
		}
		glutPostRedisplay();
	}
	else if(camState == spiral){
		if (reset){
			reset = false;
			eye = S0;
			theta = PI/2;
		}
		else if( (eye.y-S1.y) > 0 ){
			eye.y -= length(S1-S0)/spiralTimescale;
			
			theta += 2*PI/spiralTimescale;
			if(theta>2*PI){
				theta = theta - 2*PI;
			}
			eye.x = spiralRadius*cos(theta);
			eye.z = spiralRadius*sin(theta);
		}
		glutPostRedisplay();
	}
}
//----------------------------------------------------------------------------

int main( int argc, char **argv )
{
	glutInit( &argc, argv );
	glutInitWindowSize( 512, 512 );

#ifdef __APPLE__
	glutInitDisplayMode( GLUT_3_2_CORE_PROFILE | GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH );
#else
	// If you are using freeglut, the next two lines will check if 
	// the code is truly 3.2. Otherwise, comment them out
	glutInitContextVersion( 3, 2 );
	glutInitContextProfile( GLUT_CORE_PROFILE );
	glutInitDisplayMode( GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH );

#endif

	glutCreateWindow( "Assignment 3, Question 1" );
	glewExperimental = GL_TRUE;
	glewInit();

	init();
	glutDisplayFunc( display );
	glutKeyboardFunc( keyboard );
	glutReshapeFunc( reshape );
	glutIdleFunc( idle );
	glutMainLoop();
	return 0;
}


//----------------------------------------------------------------------------------------
