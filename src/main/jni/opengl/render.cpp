#include <jni.h>

#include <stdio.h>
#include <string.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "Matrix.h"

#include "image.h"
#include "loger.h"
#include "render.h"

typedef struct opengl_t {
	GLuint	m_hProgramObject;
	GLuint	m_nTextureIds[2];
	GLuint	m_nBufs[3];
	GLfloat m_ViewMatrix[16];
	GLfloat m_ObjectMatrix[16];
	GLfloat m_Colors[4];
	GLfloat m_pFloatData[8];
	int m_FloatDataSize;
	int	m_bTexInit;
	int	m_bMirror;
	int	m_nDisplayOrientation;
	int	m_nPixelFormat;
}OPENGLES, *LPOPENGLES;


/**
 *  When an array element i is transferred to the GL by the DrawArrays or DrawElements commands, 
 *  each generic attribute is expanded to four components. If size is one then the x component 
 *  of the attribute is specified by the array; the y, z, and w components are implicitly set to
 *  zero, zero, and one, respectively. If size is two then the x and y components of the attribute
 *  are specified by the array; the z, and w components are implicitly set to zero, and one, 
 *  respectively. If size is three then x, y, and z are specified, and w is implicitly set to one. 
 *  If size is four then all components are specified.
 *  
 */
const char* pVertexShaderStr =
"attribute vec4 a_position;   								\n \
attribute vec2 a_texCoord;   								\n \
varying highp vec2 v_texCoord; 								\n \
void main()                  								\n \
{                            								\n \
	gl_Position = a_position; 								\n \
	v_texCoord = a_texCoord;  								\n \
}                            								\n";


const char* pVertexShaderStrMatrix =
"uniform mat4 u_MVMatrix;					       				\n \
uniform mat4 u_PMatrix;						    			\n \
attribute vec4 a_position;							    	\n \
void main() {									    			\n \
	gl_Position = u_PMatrix * u_MVMatrix * a_position;		\n \
}													   			\n";


const char* pFragmentShaderYUYV =
"precision highp float;										\n \
uniform sampler2D y_texture;									\n \
uniform sampler2D uv_texture;								\n \
varying highp vec2 v_texCoord;								\n \
void main()													\n \
{																\n \
    mediump vec3 yuv;											\n \
    highp vec3 rgb; 											\n \
    yuv.x = texture2D(y_texture, v_texCoord).r;  			\n \
    yuv.y = texture2D(uv_texture, v_texCoord).g-0.5;		\n \
    yuv.z = texture2D(uv_texture, v_texCoord).a-0.5;		\n \
    rgb = mat3(      1,       1,       1,					\n \
              0, -0.344, 1.770,								\n \
              1.403, -0.714,       0) * yuv;				\n \
    gl_FragColor = vec4(rgb, 1);								\n \
}																\n";

const char* pFragmentShaderNV21 =
"precision highp float;										\n \
uniform sampler2D y_texture;									\n \
uniform sampler2D uv_texture;								\n \
varying highp vec2 v_texCoord;								\n \
void main()													\n \
{			 													\n \
    mediump vec3 yuv;											\n \
    highp vec3 rgb; 											\n \
    yuv.x = texture2D(y_texture, v_texCoord).r;  			\n \
    yuv.y = texture2D(uv_texture, v_texCoord).a-0.5;		\n \
    yuv.z = texture2D(uv_texture, v_texCoord).r-0.5;		\n \
    rgb = mat3(      1,       1,       1,					\n \
              0, -0.344, 1.770,								\n \
              1.403, -0.714,       0) * yuv;				\n \
    gl_FragColor = vec4(rgb, 1);								\n \
}																\n";

const char* pFragmentShaderNV12 =
"precision highp float; 										\n	\
uniform sampler2D y_texture;									\n \
uniform sampler2D uv_texture;								\n \
varying highp vec2 v_texCoord;								\n \
void main()													\n \
{																\n \
    mediump vec3 yuv;											\n \
    highp vec3 rgb; 											\n \
    yuv.x = texture2D(y_texture, v_texCoord).r;  			\n \
    yuv.y = texture2D(uv_texture, v_texCoord).r-0.5;		\n \
    yuv.z = texture2D(uv_texture, v_texCoord).a-0.5;		\n \
    rgb = mat3(      1,       1,       1,					\n \
              0, -0.344, 1.770,								\n \
              1.403, -0.714,       0) * yuv;				\n \
    gl_FragColor = vec4(rgb, 1);								\n \
}																\n";

const char* pFragmentShaderColor =
"precision mediump float;										\n \
uniform vec4 vColor;											\n \
void main()													\n \
{																\n \
 	gl_FragColor = vColor;									\n \
}																\n";

static GLuint LoadShader(GLenum shaderType, const char* pSource);

int GLDrawInit(int mirror, int ori, int format)
{
	LPOPENGLES engine;
	GLuint	vertexShader;
	GLuint	fragmentShader;
	GLint	linked;

	LOGD("glesInit() <--- format = %d", format);

	engine = (LPOPENGLES)malloc(sizeof(OPENGLES));
	engine->m_hProgramObject		= 0;
	engine->m_bTexInit				= -1;
	engine->m_bMirror				= mirror;
	engine->m_nDisplayOrientation	= ori;
	engine->m_nPixelFormat 			= format;

	vertexShader = LoadShader(GL_VERTEX_SHADER, pVertexShaderStrMatrix);
	fragmentShader = LoadShader(GL_FRAGMENT_SHADER, pFragmentShaderColor);

	engine->m_hProgramObject = glCreateProgram();

	glAttachShader(engine->m_hProgramObject, vertexShader);
	glAttachShader(engine->m_hProgramObject, fragmentShader);

	glBindAttribLocation(engine->m_hProgramObject, 0, "a_position");

	glLinkProgram(engine->m_hProgramObject);

	LOGD("glLinkProgram");

	glGetProgramiv( engine->m_hProgramObject, GL_LINK_STATUS, &linked);
	if (0 == linked) {
		GLint	infoLen = 0;
		LOGE("link failed");
		glGetProgramiv( engine->m_hProgramObject, GL_INFO_LOG_LENGTH, &infoLen);

		if (infoLen > 1) {
			char* infoLog = (char*)malloc(sizeof(char) * infoLen);

			glGetProgramInfoLog( engine->m_hProgramObject, infoLen, NULL, infoLog);
			LOGE( "Error linking program: %s", infoLog);

			free(infoLog);
			infoLog = NULL;
		}

		glDeleteProgram( engine->m_hProgramObject);
		return 0;
	}

	glValidateProgram(engine->m_hProgramObject);
	glGetProgramiv( engine->m_hProgramObject, GL_VALIDATE_STATUS, &linked);
	if (linked == 0) {
		LOGE("program failed");
		return 0;
	}

	//VBO
	glGenBuffers(1, engine->m_nBufs);
	glBindBuffer(GL_ARRAY_BUFFER, engine->m_nBufs[0]);

	LOGD("glesInit() --->");
	return (int)engine;
}

int GLImageInit(int mirror, int ori, int format)
{
	LPOPENGLES engine;
	GLuint	vertexShader;
	GLuint	fragmentShader;
	GLint	linked;

	LOGD("glesInit() <--- format = %d", format);

	engine = (LPOPENGLES)malloc(sizeof(OPENGLES));
	engine->m_hProgramObject		= 0;
	engine->m_bTexInit				= -1;
	engine->m_bMirror				= mirror;
	engine->m_nDisplayOrientation	= ori;
	engine->m_nPixelFormat 			= format;

	vertexShader = LoadShader(GL_VERTEX_SHADER, pVertexShaderStr);
	if (engine->m_nPixelFormat == CP_PAF_NV21) {
		fragmentShader = LoadShader(GL_FRAGMENT_SHADER, pFragmentShaderNV21);
	} else if (engine->m_nPixelFormat == CP_PAF_NV12) {
		fragmentShader = LoadShader(GL_FRAGMENT_SHADER, pFragmentShaderNV12);
	} else if (engine->m_nPixelFormat == CP_PAF_YUYV) {
		fragmentShader = LoadShader(GL_FRAGMENT_SHADER, pFragmentShaderYUYV);
	}

	engine->m_hProgramObject = glCreateProgram();
	if (0 == engine->m_hProgramObject) {
		LOGE("create programObject failed");
		return 0;
	}

	LOGD("glAttachShader");

	glAttachShader(engine->m_hProgramObject, vertexShader);
	glAttachShader(engine->m_hProgramObject, fragmentShader);

	LOGD("glBindAttribLocation");
	glBindAttribLocation(engine->m_hProgramObject, 0, "a_position");
	glBindAttribLocation(engine->m_hProgramObject, 1, "a_texCoord");

	glLinkProgram ( engine->m_hProgramObject );

	LOGD("glLinkProgram");

	glGetProgramiv( engine->m_hProgramObject, GL_LINK_STATUS, &linked);
	if (0 == linked) {
		GLint	infoLen = 0;
		LOGE("link failed");
		glGetProgramiv( engine->m_hProgramObject, GL_INFO_LOG_LENGTH, &infoLen);

		if (infoLen > 1) {
			char* infoLog = (char*)malloc(sizeof(char) * infoLen);

			glGetProgramInfoLog( engine->m_hProgramObject, infoLen, NULL, infoLog);
			LOGE( "Error linking program: %s", infoLog);

			free(infoLog);
			infoLog = NULL;
		}

		glDeleteProgram( engine->m_hProgramObject);
		return 0;
	}

	//glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	//glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	//glEnable(GL_TEXTURE_2D);

	LOGD("glGenTextures");
	// Textures
	glGenTextures(2, engine->m_nTextureIds);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, engine->m_nTextureIds[0]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, engine->m_nTextureIds[1]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	LOGD("VBO");

	//VBO
	glGenBuffers(3, engine->m_nBufs);
	GLfloat vScale = 1.0;
	GLfloat vVertices[] = { -vScale,  vScale, 0.0f, //1.0f,  // Position 0
                            -vScale, -vScale, 0.0f, //1.0f, // Position 1
                             vScale, -vScale, 0.0f, //1.0f, // Position 2
                             vScale,  vScale, 0.0f, //1.0f,  // Position 3
                         };

	GLfloat tCoords[] = {0.0f,  0.0f,
						 0.0f,  1.0f,
						 1.0f,  1.0f,
						 1.0f,  0.0f};

	int degree = 0;

	while (engine->m_nDisplayOrientation > degree) {
		GLfloat temp[2];
		degree += 90;
		temp[0] = tCoords[0]; temp[1] = tCoords[1];
		tCoords[0] = tCoords[2]; tCoords[1] = tCoords[3];
		tCoords[2] = tCoords[4]; tCoords[3] = tCoords[5];
		tCoords[4] = tCoords[6]; tCoords[5] = tCoords[7];
		tCoords[6] = temp[0]; tCoords[7] = temp[1];
	}

	if (engine->m_nDisplayOrientation == 0 || engine->m_nDisplayOrientation == 180) {
		if (engine->m_bMirror == 1){
			GLfloat temp[2];
			LOGD("set mirror is true");
			temp[0] = tCoords[0]; temp[1] = tCoords[2];
			tCoords[0] = tCoords[4]; tCoords[2] = tCoords[6];
			tCoords[4] = temp[0]; tCoords[6] = temp[1];
		}
	} else {
		if (engine->m_bMirror == 1){
			GLfloat temp[2];
			LOGD("set mirror is true");
			temp[0] = tCoords[1]; temp[1] = tCoords[3];
			tCoords[1] = tCoords[5]; tCoords[3] = tCoords[7];
			tCoords[5] = temp[0]; tCoords[7] = temp[1];
		}
	}

	GLushort indexs[] = { 0, 1, 2, 0, 2, 3 };

	glBindBuffer(GL_ARRAY_BUFFER, engine->m_nBufs[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vVertices), vVertices, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, engine->m_nBufs[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(tCoords), tCoords, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, engine->m_nBufs[2]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indexs), indexs, GL_STATIC_DRAW);

	LOGD("glesInit() --->");

	return (int)engine;
}

void GLChanged(int handle, int w, int h)
{
	LPOPENGLES engine = (LPOPENGLES)handle;
	LOGD("glesChanged(%d, %d) <---", w, h);
	engine->m_bTexInit = -1;
	glViewport(0, 0, w, h);
	LOGD("glesChanged() --->");
}

void GLDrawLines( int handle, int w, int h, int *pos, int len)
{
	LPOPENGLES engine = (LPOPENGLES)handle;
	if (engine == NULL) {
		LOGE("engine == MNull");
		return;
	}

	const GLfloat ratio = (GLfloat) w / h;
	const float left = 	-0.5f;		//-ratio;
	const float right =	0.5f;		//ratio;
	const float bottom = 	-0.5f;		//-1.0f;
	const float top = 		0.5f;		//1.0f;
	const float near = 	2;			//1.0f;
	const float far = 		6;			//10.0f;


	Matrix::matrixFrustumM((float*)engine->m_ObjectMatrix, left, right, bottom, top, near, far);

	const GLfloat eyeX = 0.0f;
	const GLfloat eyeY = 0.0f;
	const GLfloat eyeZ = 4.0f;

	const GLfloat lookX = 0.0f;
	const GLfloat lookY = 0.0f;
	const GLfloat lookZ = 0.0f;

	const GLfloat upX = 0.0f;
	const GLfloat upY = 1.0f;
	const GLfloat upZ = 0.0f;
	Matrix::matrixLookAtM(engine->m_ViewMatrix, eyeX, eyeY, eyeZ, lookX, lookY, lookZ, upX, upY, upZ);

	Matrix::matrixRotateM(engine->m_ViewMatrix, 360 - 0, 0, 0, 1);

	// use shader
	glUseProgram ( engine->m_hProgramObject );

	engine->m_Colors[0] = 255;
	engine->m_Colors[1] = 0;
	engine->m_Colors[2] = 0;
	engine->m_Colors[3] = 1.0f;

	GLuint m1 = glGetUniformLocation(engine->m_hProgramObject, "u_MVMatrix");
	GLuint m2 = glGetUniformLocation(engine->m_hProgramObject, "u_PMatrix");
	GLuint c1 = glGetUniformLocation(engine->m_hProgramObject, "vColor");

	glUniformMatrix4fv(m1, 1, GL_FALSE, engine->m_ViewMatrix);
	glUniformMatrix4fv(m2, 1, GL_FALSE, engine->m_ObjectMatrix);
	glUniform4fv(c1, 1, engine->m_Colors);

	for (int i = 0; i < len; i += 2){
		engine->m_pFloatData[i] = ((2.0f * pos[i]) / (w - 1)) - 1.0f;
		engine->m_pFloatData[i + 1] = 1.0f - ((2.0f * pos[i + 1]) / (h - 1));
	}

	glLineWidth(4.0f);

	glBindBuffer(GL_ARRAY_BUFFER, engine->m_nBufs[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(engine->m_pFloatData), engine->m_pFloatData, GL_DYNAMIC_DRAW);
	glVertexAttribPointer ( 0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), 0 );
	glEnableVertexAttribArray(0);
	glDrawArrays(GL_LINE_LOOP, 0, 4);
	glDisableVertexAttribArray(0);
}

void GLImageRender(int handle, unsigned char* pData, int w, int h)
{
	LPOPENGLES engine = (LPOPENGLES)handle;
	if (pData == NULL) {
		LOGE("pOffScreen == MNull");
		return;
	}

	//clean
	glClear ( GL_COLOR_BUFFER_BIT );

	//Texture -> GPU
	if (engine->m_nPixelFormat == CP_PAF_NV21 || engine->m_nPixelFormat == CP_PAF_NV12) {
		glBindTexture(GL_TEXTURE_2D, engine->m_nTextureIds[0]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, w, h, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, pData);

		glBindTexture(GL_TEXTURE_2D, engine->m_nTextureIds[1]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, w >> 1, h >> 1, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, pData + w * h);
	} else if (engine->m_nPixelFormat == CP_PAF_YUYV) {
		glBindTexture(GL_TEXTURE_2D, engine->m_nTextureIds[0]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, w, h, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, pData);

		glBindTexture(GL_TEXTURE_2D, engine->m_nTextureIds[1]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w >> 1, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pData);
	}
	// use shader
	glUseProgram ( engine->m_hProgramObject );

	GLuint textureUniformY = glGetUniformLocation(engine->m_hProgramObject, "y_texture");
	GLuint textureUniformU = glGetUniformLocation(engine->m_hProgramObject, "uv_texture");

	glBindTexture(GL_TEXTURE_2D, engine->m_nTextureIds[0]);
	glUniform1i(textureUniformY, 0);

	glBindTexture(GL_TEXTURE_2D, engine->m_nTextureIds[1]);
	glUniform1i(textureUniformU, 1);

	glBindBuffer(GL_ARRAY_BUFFER, engine->m_nBufs[0]);
	glVertexAttribPointer ( 0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), 0 );
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, engine->m_nBufs[1]);
	glVertexAttribPointer ( 1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), 0 );
	glEnableVertexAttribArray(1);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, engine->m_nBufs[2]);
	glDrawElements ( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0 );

	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);

}

void GLUnInit(int handle)
{
	LPOPENGLES engine = (LPOPENGLES)handle;
	free(engine);
}


GLuint LoadShader(GLenum shaderType, const char* pSource)
{
    GLuint shader = 0;
	shader = glCreateShader(shaderType);
	LOGD("glGetShaderiv called  shader = %d GL_INVALID_ENUM = %d GL_INVALID_OPERATION = %d", shader, GL_INVALID_ENUM, GL_INVALID_OPERATION);
    if (shader) {
        glShaderSource(shader, 1, &pSource, NULL);
        glCompileShader(shader);
        GLint compiled = 1;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        LOGD( "glGetShaderiv called compiled = %d, shader = %d", compiled, shader);
        if (!compiled)
		{
            GLint infoLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen)
			{
                char* buf = (char*) malloc(infoLen);
                if (buf)
				{
                    glGetShaderInfoLog(shader, infoLen, NULL, buf);
                    LOGE("Could not compile shader %d: %s",
                            shaderType, buf);
                    free(buf);
                }
                glDeleteShader(shader);
                shader = 0;
            }
			return 0;
        }
    }
    return shader;
}


