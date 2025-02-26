#include "CameraRenderer.h"
#include <opencv2/opencv.hpp>

#include "CameraUtil.h"
#include "Logwrapper.h"

GLfloat CameraRenderer::mVertices[20];

const GLfloat CameraRenderer::mLandscapeVertices[] =
{
   -1.0f, 1.0f, 0.0f, // Position 0
   0.0f, 0.0f, // TexCoord 0
   -1.0f, -1.0f, 0.0f, // Position 1
   0.0f, 1.0f, // TexCoord 1
   1.0f, 1.0f, 0.0f, // Position 2
   1.0f, 0.0f, // TexCoord 2
   1.0f, -1.0f, 0.0f, // Position 3
   1.0f, 1.0f // TexCoord 3
};

int getNextPowerOfTwo(int nValue)
{
   if(nValue<=1)		return 1;
   if(nValue<=2)		return 2;
   if(nValue<=4)		return 4;
   if(nValue<=8)		return 8;
   if(nValue<=16)		return 16;
   if(nValue<=32)		return 32;
   if(nValue<=64)		return 64;
   if(nValue<=128)		return 128;
   if(nValue<=256)		return 256;
   if(nValue<=512)		return 512;
   if(nValue<=1024)	return 1024;
   if(nValue<=2048)	return 2048;
   if(nValue<=4096)	return 4096;
   if(nValue<=8192)	return 8192;
   if(nValue<=16384)	return 16384;
   if(nValue<=32768)	return 32768;
   if(nValue<=65536)	return 65536;

   return 0;		// failure, too large number
}


CameraRenderer::CameraRenderer(bool npotTextures) {
	mInitialized = false;
	mNPOTTextures = npotTextures;
	mWidth = 0;
	mHeight = 0;
}

CameraRenderer::~CameraRenderer() {
	UnInit();
}

bool CameraRenderer::Init() {
	//Clean previous state if any
	UnInit();
	mInitialized = InitGPU();

	return mInitialized;
}

bool CameraRenderer::InitGPU() {
	if (!CreateShaderProgram()) {
		LOGE("Unable to create shader program.");
		return false;
	}

	if (!CreateTextures()) {
		LOGE("Unable to create textures.");
		return false;
	}

	return true;
}

bool CameraRenderer::CreateShaderProgram() {
	// vertex shader source
	const char* vShaderStr =
			"precision highp float;    		  \n"
			"attribute vec4  a_v4Position;    \n"
			"attribute vec2  a_v2TexCoord;    \n"
			"varying   vec2  v_v2TexCoord;    \n"
			"void main()                      \n"
			"{                                \n"
			"    gl_Position  = a_v4Position; \n"
			"    v_v2TexCoord = a_v2TexCoord; \n"
			"}                                \n";

	const char* fShaderStr =
			"precision highp     float;				                 \n"
			"uniform   sampler2D rgbImg;				             \n"
			"varying   vec2      v_v2TexCoord;			             \n"
			"void main()						                     \n"
			"{							                             \n"
			"    gl_FragColor = texture2D( rgbImg, v_v2TexCoord );	 \n"
			"}							                             \n";

	mProgramObject = CameraUtil::CompileAndLinkShaderProgram(vShaderStr,
			fShaderStr);

	if (!mProgramObject) {
		return false;
	}

	mGL_u_ImgRGBLoc = glGetUniformLocation(mProgramObject, "rgbImg");
	mGL_av4_PositionLoc = glGetAttribLocation(mProgramObject, "a_v4Position");
	mGL_av2_TexCoordLoc = glGetAttribLocation(mProgramObject, "a_v2TexCoord");
	return true;
}

bool CameraRenderer::CreateTextures() {

	glGenTextures(1, &(mTextureId[0]));

	if (CameraUtil::checkGlError("glGenTextures-Y")){
		return false;
	}
	glBindTexture(GL_TEXTURE_2D, mTextureId[0]);
	if (CameraUtil::checkGlError("glBindTexture-Y")){
		return false;
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// Use landscape mode per default
	for (size_t i = 0; i < 20; i++)
		mVertices[i] = mLandscapeVertices[i];

	if (!mNPOTTextures) {
		int texWidth = getNextPowerOfTwo(mWidth);
		int texHeight = getNextPowerOfTwo(mHeight);

		float texCoordWidth = mWidth / (float) texWidth;
		float texCoordHeight = mHeight / (float) texHeight;

		// Set the texture coords to the correct values
		// (t,r) are represented as their positions in the
		// combined vertex/texture coord array

		// (3,4) = (0,0) is already set
		// (8,9) = (0,1) -> (0, actual height)
		mVertices[9] = texCoordHeight;
		// (13,14) = (1,0) -> (actual width, 0)
		mVertices[13] = texCoordWidth;
		// (18,19) = (1,1) -> (actual width, actual height)
		mVertices[18] = texCoordWidth;
		mVertices[19] = texCoordHeight;

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texWidth, texHeight, 0, GL_RGB,
				GL_UNSIGNED_SHORT_5_6_5, NULL);

		if (CameraUtil::checkGlError("glTexImage2D-RGB")) {
			LOGE("mWidth %d, mHeight %d", mWidth, mHeight);
			return false;
		}
	}

	return true;
}

void CameraRenderer::UnInit() {
	if (mInitialized) {
		glDeleteTextures(1, mTextureId);

		if (!mProgramObject) {
			glDeleteProgram(mProgramObject);
			CameraUtil::checkGlError("glDeleteProgram");
		}

		mInitialized = false;
	}
}

bool CameraRenderer::Render(const uint8_t* img, uint16_t w, uint16_t h) {

	if (w != mWidth || h != mHeight) {
		mWidth = w;
		mHeight = h;
		UnInit();
	}

	if (!mInitialized && !Init()) {
		LOGE("CameraRenderer::Initialize FAILED");
		return false;
	}

	if (NULL == img) {
		LOGE("NULL Image Buffer. Can't render camera image.");
		return false;
	}

	//Make sure that previous OpenGL Error have been cleaned.
	glGetError();

	// needed for unity compatiblity
	glDisable (GL_DEPTH_TEST);
	glDisable (GL_CULL_FACE);
	//Unity does not unbind these buffers before we are called, so we have to do so.
	//This should shouldn't have an effect on other rendering solutions as long as
	//the programmer is binding these buffers properly
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glUseProgram(mProgramObject);
	CameraUtil::checkGlError("glUseProgram");

	// Load vertex data
	glVertexAttribPointer(mGL_av4_PositionLoc, 3, GL_FLOAT, GL_FALSE,
			5 * sizeof(float), mVertices);
	CameraUtil::checkGlError("glVertexAttribPointer");
	glVertexAttribPointer(mGL_av2_TexCoordLoc, 2, GL_FLOAT, GL_FALSE,
			5 * sizeof(float), &mVertices[3]);
	CameraUtil::checkGlError("glVertexAttribPointer");

	glEnableVertexAttribArray(mGL_av4_PositionLoc);
	glEnableVertexAttribArray(mGL_av2_TexCoordLoc);

	//Update Texture Image.
	UpdateTextures(img, w, h);

	glActiveTexture (GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, mTextureId[0]);

	if (CameraUtil::checkGlError("glBindTexture-RGB")) {
	}

	glUniform1i(mGL_u_ImgRGBLoc, 0);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	CameraUtil::checkGlError("glDrawArrays");

	glDisableVertexAttribArray(mGL_av4_PositionLoc);
	glDisableVertexAttribArray(mGL_av2_TexCoordLoc);

	return true;

}

bool CameraRenderer::UpdateTextures(const uint8_t* rgbImg, uint16_t w, uint16_t h) {

	glActiveTexture (GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, mTextureId[0]);

	if (CameraUtil::checkGlError("glBindTexture-RGB")){
		return false;
	}
	if (mNPOTTextures) {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB,
				GL_UNSIGNED_SHORT_5_6_5, rgbImg);
		if (CameraUtil::checkGlError("glTexImage2D-RGB")){
			return false;
		}
	} else {
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGB,
				GL_UNSIGNED_SHORT_5_6_5, rgbImg);
		if (CameraUtil::checkGlError("glTexSubImage2D-RGB")){
			return false;
		}
	}
	return true;
}
