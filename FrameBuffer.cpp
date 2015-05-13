#ifndef __LINUX__
# include <windows.h>
#else
# include "winlnxdefs.h"
#endif // __LINUX__
#include <assert.h>
#include "OpenGL.h"
#include "FrameBuffer.h"
#include "DepthBuffer.h"
#include "RSP.h"
#include "RDP.h"
#include "gDP.h"
#include "VI.h"
#include "Textures.h"
#include "Combiner.h"
#include "Types.h"
#include "Debug.h"

bool g_bCopyToRDRAM = true;
bool g_bCopyFromRDRAM = false;
FrameBufferInfo frameBuffer;

class FrameBufferToRDRAM
{
public:
	FrameBufferToRDRAM() :
	  m_FBO(0), m_pTexture(NULL), m_curIndex(0)
	{
		m_aPBO[0] = m_aPBO[1] = 0;
		m_aAddress[0] = m_aAddress[1] = 0;
	}

	void Init();
	void Destroy();

	void CopyToRDRAM( u32 address, bool bSync );
	void CopyAuxBufferToRDRAM( u32 address );

private:
	struct RGBA {
		u8 r, g, b, a;
	};

	GLuint m_FBO;
	CachedTexture * m_pTexture;
	GLuint m_aPBO[2];
	u32 m_aAddress[2];
	u32 m_curIndex;
};

class RDRAMtoFrameBuffer
{
public:
	RDRAMtoFrameBuffer() : m_pTexture(NULL), m_PBO(0) {}

	void Init();
	void Destroy();

	void CopyFromRDRAM( u32 _address, bool _bUseAlpha);

private:
	struct PBOBinder {
		PBOBinder(GLuint _PBO)
		{
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, _PBO);
		}
		~PBOBinder() {
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
		}
	};
	CachedTexture * m_pTexture;
	GLuint m_PBO;
};

FrameBufferToRDRAM g_fbToRDRAM;
RDRAMtoFrameBuffer g_RDRAMtoFB;

void FrameBuffer_Init()
{
	frameBuffer.current = NULL;
	frameBuffer.top = NULL;
	frameBuffer.bottom = NULL;
	frameBuffer.numBuffers = 0;
	frameBuffer.drawBuffer = GL_BACK;
	g_fbToRDRAM.Init();
	g_RDRAMtoFB.Init();
}

void FrameBuffer_RemoveBottom()
{
	FrameBuffer *newBottom = frameBuffer.bottom->higher;

	TextureCache_Remove( frameBuffer.bottom->texture );
	if (frameBuffer.bottom->fbo != 0)
		ogl_glDeleteFramebuffers(1, &frameBuffer.bottom->fbo);

	if (frameBuffer.bottom == frameBuffer.top)
		frameBuffer.top = NULL;

	free( frameBuffer.bottom );

    frameBuffer.bottom = newBottom;
	
	if (frameBuffer.bottom != NULL)
		frameBuffer.bottom->lower = NULL;

	frameBuffer.numBuffers--;
}

void FrameBuffer_Remove( FrameBuffer *buffer )
{
	if ((buffer == frameBuffer.bottom) &&
		(buffer == frameBuffer.top))
	{
		frameBuffer.top = NULL;
		frameBuffer.bottom = NULL;
	}
	else if (buffer == frameBuffer.bottom)
	{
		frameBuffer.bottom = buffer->higher;

		if (frameBuffer.bottom)
			frameBuffer.bottom->lower = NULL;
	}
	else if (buffer == frameBuffer.top)
	{
		frameBuffer.top = buffer->lower;

		if (frameBuffer.top)
			frameBuffer.top->higher = NULL;
	}
	else
	{
		buffer->higher->lower = buffer->lower;
		buffer->lower->higher = buffer->higher;
	}

	if (buffer->texture)
		TextureCache_Remove( buffer->texture );
	if (buffer->fbo != 0)
		ogl_glDeleteFramebuffers(1, &buffer->fbo);

	free( buffer );

	frameBuffer.numBuffers--;
}

void FrameBuffer_RemoveBuffer( u32 address )
{
	FrameBuffer *current = frameBuffer.bottom;

	while (current != NULL)
	{
		if (current->startAddress == address)
		{
			//current->texture = NULL;
			FrameBuffer_Remove( current );
			return;
		}
		current = current->higher;
	}
}

FrameBuffer *FrameBuffer_AddTop()
{
	FrameBuffer *newtop = (FrameBuffer*)malloc( sizeof( FrameBuffer ) );

	newtop->texture = TextureCache_AddTop();
	newtop->fbo = 0;

	newtop->lower = frameBuffer.top;
	newtop->higher = NULL;

	if (frameBuffer.top)
		frameBuffer.top->higher = newtop;

	if (!frameBuffer.bottom)
		frameBuffer.bottom = newtop;

    frameBuffer.top = newtop;

	frameBuffer.numBuffers++;

	return newtop;
}

void FrameBuffer_MoveToTop( FrameBuffer *newtop )
{
	if (newtop == frameBuffer.top)
		return;

	if (newtop == frameBuffer.bottom)
	{
		frameBuffer.bottom = newtop->higher;
		frameBuffer.bottom->lower = NULL;
	}
	else
	{
		newtop->higher->lower = newtop->lower;
		newtop->lower->higher = newtop->higher;
	}

	newtop->higher = NULL;
	newtop->lower = frameBuffer.top;
	frameBuffer.top->higher = newtop;
	frameBuffer.top = newtop;

	TextureCache_MoveToTop( newtop->texture );
}

void FrameBuffer_Destroy()
{
	while (frameBuffer.bottom)
		FrameBuffer_RemoveBottom();
	g_fbToRDRAM.Destroy();
	g_RDRAMtoFB.Destroy();
}

void FrameBuffer_SaveBuffer( u32 address, u16 format, u16 size, u16 width, u16 height )
{
	frameBuffer.drawBuffer = GL_DRAW_FRAMEBUFFER;
	FrameBuffer *current = frameBuffer.top;
	if (current != NULL && gDP.colorImage.height > 1) {
		current->endAddress = current->startAddress + (((current->width * gDP.colorImage.height) << current->size >> 1) - 1);
		if (current->startAddress == 0x13ba50 || current->startAddress == 0x264430) { // HACK ALERT: Dirty hack for Mario Tennis score board
			g_fbToRDRAM.CopyAuxBufferToRDRAM(current->startAddress);
			FrameBuffer_Remove( current );
			current = NULL;
		} else if (!g_bCopyToRDRAM && !current->cleared)
			gDPFillRDRAM(current->startAddress, 0, 0, current->width, gDP.colorImage.height, current->width, current->size, frameBuffer.top->fillcolor);
	}

	current = FrameBuffer_FindBuffer(address);
	if (current != NULL) {
		if ((current->startAddress != address) ||
			(current->width != width) ||
			//(current->height != height) ||
			//(current->size != size) ||  // TODO FIX ME
			(current->scaleX != OGL.scaleX) ||
			(current->scaleY != OGL.scaleY))
		{
			FrameBuffer_Remove( current );
			current = NULL;
		} else {
			FrameBuffer_MoveToTop( current );
			ogl_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, current->fbo);
			if (current->size != size) {
				f32 fillColor[4];
				gDPGetFillColor(fillColor);
				OGL_ClearColorBuffer(fillColor);
				current->size = size;
				current->texture->format = format;
				current->texture->size = size;
			}
		}
	}
	if  (current == NULL) {
		// Wasn't found or removed, create a new one
		current = FrameBuffer_AddTop();

		current->startAddress = address;
		current->endAddress = address + ((width * height << size >> 1) - 1);
		current->width = width;
		current->height = height;
		current->size = size;
		current->scaleX = OGL.scaleX;
		current->scaleY = OGL.scaleY;
		current->fillcolor = 0;

		current->texture->width = (u32)(current->width * OGL.scaleX);
		current->texture->height = (u32)(current->height * OGL.scaleY);
		current->texture->format = format;
		current->texture->size = size;
		current->texture->clampS = 1;
		current->texture->clampT = 1;
		current->texture->address = current->startAddress;
		current->texture->clampWidth = current->width;
		current->texture->clampHeight = current->height;
		current->texture->frameBufferTexture = TRUE;
		current->texture->maskS = 0;
		current->texture->maskT = 0;
		current->texture->mirrorS = 0;
		current->texture->mirrorT = 0;
		current->texture->realWidth = (u32)pow2( current->texture->width );
		current->texture->realHeight = (u32)pow2( current->texture->height );
		current->texture->textureBytes = current->texture->realWidth * current->texture->realHeight * 4;
		cache.cachedBytes += current->texture->textureBytes;

		glBindTexture( GL_TEXTURE_2D, current->texture->glName );
		if (size > G_IM_SIZ_8b)
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, current->texture->realWidth, current->texture->realHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		else
			glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, current->texture->realWidth, current->texture->realHeight, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		ogl_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		ogl_glGenFramebuffers(1, &current->fbo);
		ogl_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, current->fbo);
		ogl_glFramebufferTexture(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, current->texture->glName, 0);
	}

	if (depthBuffer.top != NULL && depthBuffer.top->renderbuf > 0) {
		ogl_glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer.top->renderbuf);
		ogl_glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer.top->renderbuf);
	}

	GLuint attachments[2] = { GL_COLOR_ATTACHMENT0, GL_DEPTH_ATTACHMENT };
	ogl_glDrawBuffers(2,  attachments, current->texture->glName);
	assert(checkFBO());

#ifdef DEBUG
	DebugMsg( DEBUG_HIGH | DEBUG_HANDLED, "FrameBuffer_SaveBuffer( 0x%08X ); depth buffer is 0x%08X\n",
		address, (depthBuffer.top != NULL && depthBuffer.top->renderbuf > 0) ? depthBuffer.top->address : 0
	);
#endif
	*(u32*)&RDRAM[current->startAddress] = current->startAddress;

	current->cleared = false;

	gSP.changed |= CHANGED_TEXTURE;
}


#if 1
void FrameBuffer_RenderBuffer( u32 address )
{
	FrameBuffer *current = FrameBuffer_FindBuffer(address);
	if (current == NULL)
		return;
	ogl_glBindFramebuffer(GL_READ_FRAMEBUFFER, current->fbo);
	ogl_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glDrawBuffer( GL_FRONT );
	ogl_glBlitFramebuffer(
//				0, 0, current->texture->realWidth, current->texture->realHeight,
		0, 0, OGL.width, OGL.height,
		0, OGL.heightOffset, OGL.width, OGL.height+OGL.heightOffset,
		GL_COLOR_BUFFER_BIT, GL_LINEAR
	);
	glDrawBuffer( GL_BACK );
	ogl_glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	ogl_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frameBuffer.top->fbo);
}
#else

void FrameBuffer_RenderBuffer( u32 address )
{
	FrameBuffer *current = frameBuffer.top;

	while (current != NULL)
	{
		if ((current->startAddress <= address) &&
			(current->endAddress >= address))
		{
			/*
			float fill_color[4] = {1.0f, 0.0f, 0.0f, 1.0f};
			ogl_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, current->fbo);
			ogl_glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer.top->renderbuf);
			ogl_glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer.top->renderbuf);
			GLuint attachments[2] = { GL_COLOR_ATTACHMENT0, GL_DEPTH_ATTACHMENT };
			ogl_glDrawBuffers(2,  attachments, current->texture->glName);
			assert(checkFBO());
			OGL_ClearDepthBuffer();
			OGL_ClearColorBuffer(fill_color);
			ogl_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frameBuffer.top->fbo);
			*/

			glPushAttrib( GL_ENABLE_BIT | GL_VIEWPORT_BIT );

			Combiner_BeginTextureUpdate();
			TextureCache_ActivateTexture( 0, current->texture );
			Combiner_SetCombine( EncodeCombineMode( 0, 0, 0, TEXEL0, 0, 0, 0, 1, 0, 0, 0, TEXEL0, 0, 0, 0, 1 ) );
/*			if (OGL.ARB_multitexture)
			{
				for (int i = 0; i < OGL.maxTextureUnits; i++)
				{
					glActiveTextureARB( GL_TEXTURE0_ARB + i );
					glDisable( GL_TEXTURE_2D );
				}

				glActiveTextureARB( GL_TEXTURE0_ARB );
			}

			TextureCache_ActivateTexture( 0, current->texture );
			glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
			glEnable( GL_TEXTURE_2D );*/

			glDisable( GL_BLEND );
			glDisable( GL_ALPHA_TEST );
			glDisable( GL_DEPTH_TEST );
			glDisable( GL_CULL_FACE );
			glDisable( GL_POLYGON_OFFSET_FILL );
//			glDisable( GL_REGISTER_COMBINERS_NV );
			glDisable( GL_FOG );

			glMatrixMode( GL_PROJECTION );
			glLoadIdentity();
 			glOrtho( 0, OGL.width, 0, OGL.height, -1.0f, 1.0f );
			glViewport( 0, OGL.heightOffset, OGL.width, OGL.height );
			glDisable( GL_SCISSOR_TEST );

			float u1, v1;

			u1 = (float)current->texture->width / (float)current->texture->realWidth;
			v1 = (float)current->texture->height / (float)current->texture->realHeight;

			ogl_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			glDrawBuffer( GL_FRONT );
			glBegin(GL_QUADS);
 				glTexCoord2f( 0.0f, 0.0f );
				glVertex2f( 0.0f, (GLfloat)(OGL.height - current->texture->height) );

				glTexCoord2f( 0.0f, v1 );
				glVertex2f( 0.0f, (GLfloat)OGL.height );

 				glTexCoord2f( u1,  v1 );
				glVertex2f( current->texture->width, (GLfloat)OGL.height );

 				glTexCoord2f( u1, 0.0f );
				glVertex2f( current->texture->width, (GLfloat)(OGL.height - current->texture->height) );
			glEnd();
			glDrawBuffer( GL_BACK );
			ogl_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frameBuffer.top->fbo);
#ifdef DEBUG
			DebugMsg( DEBUG_HIGH | DEBUG_HANDLED, "FrameBuffer_RenderBuffer( 0x%08X ); \n", address);
#endif

/*			glEnable( GL_TEXTURE_2D );
			glActiveTextureARB( GL_TEXTURE0_ARB );
			glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB );*/
			glLoadIdentity();
			glPopAttrib();

			current->changed = FALSE;

			gSP.changed |= CHANGED_TEXTURE | CHANGED_VIEWPORT;
			gDP.changed |= CHANGED_COMBINE;

			return;
		}
		current = current->lower;
	}
}
#endif

FrameBuffer *FrameBuffer_FindBuffer( u32 address )
{
	FrameBuffer *current = frameBuffer.top;

	while (current)
	{
		if ((current->startAddress <= address) &&
			(current->endAddress >= address))
			return current;
		current = current->lower;
	}

	return NULL;
}

void FrameBuffer_ActivateBufferTexture( s16 t, FrameBuffer *buffer )
{
    buffer->texture->scaleS = OGL.scaleX / (float)buffer->texture->realWidth;
    buffer->texture->scaleT = OGL.scaleY / (float)buffer->texture->realHeight;

	if (gSP.textureTile[t]->shifts > 10)
		buffer->texture->shiftScaleS = (float)(1 << (16 - gSP.textureTile[t]->shifts));
	else if (gSP.textureTile[t]->shifts > 0)
		buffer->texture->shiftScaleS = 1.0f / (float)(1 << gSP.textureTile[t]->shifts);
	else
		buffer->texture->shiftScaleS = 1.0f;

	if (gSP.textureTile[t]->shiftt > 10)
		buffer->texture->shiftScaleT = (float)(1 << (16 - gSP.textureTile[t]->shiftt));
	else if (gSP.textureTile[t]->shiftt > 0)
		buffer->texture->shiftScaleT = 1.0f / (float)(1 << gSP.textureTile[t]->shiftt);
	else
		buffer->texture->shiftScaleT = 1.0f;

	const u32 shift = gSP.textureTile[t]->imageAddress - buffer->startAddress;
	const u32 factor = buffer->width << buffer->size >> 1;
	if (gSP.textureTile[t]->loadType == LOADTYPE_TILE)
	{
		buffer->texture->offsetS = buffer->loadTile->uls;
		buffer->texture->offsetT = (float)(buffer->height - (buffer->loadTile->ult + shift/factor));
	}
	else
	{
		buffer->texture->offsetS = (float)(shift % factor);
		buffer->texture->offsetT = (float)(buffer->height - shift/factor);
	}

//	FrameBuffer_RenderBuffer(buffer->startAddress);
	TextureCache_ActivateTexture( t, buffer->texture );
	gDP.changed |= CHANGED_FB_TEXTURE;
}

void FrameBuffer_ActivateBufferTextureBG( s16 t, FrameBuffer *buffer )
{
	buffer->texture->scaleS = OGL.scaleX / (float)buffer->texture->realWidth;
	buffer->texture->scaleT = OGL.scaleY / (float)buffer->texture->realHeight;

	buffer->texture->shiftScaleS = 1.0f;
	buffer->texture->shiftScaleT = 1.0f;

	buffer->texture->offsetS = gSP.bgImage.imageX;
	buffer->texture->offsetT = (float)buffer->height - gSP.bgImage.imageY;

	//	FrameBuffer_RenderBuffer(buffer->startAddress);
	TextureCache_ActivateTexture( t, buffer->texture );
	gDP.changed |= CHANGED_FB_TEXTURE;
}

void FrameBufferToRDRAM::Init()
{
	// generate a framebuffer
	ogl_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	ogl_glGenFramebuffers(1, &m_FBO);
	ogl_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_FBO);

	m_pTexture = TextureCache_AddTop();
	m_pTexture->format = G_IM_FMT_RGBA;
	m_pTexture->clampS = 1;
	m_pTexture->clampT = 1;
	m_pTexture->frameBufferTexture = TRUE;
	m_pTexture->maskS = 0;
	m_pTexture->maskT = 0;
	m_pTexture->mirrorS = 0;
	m_pTexture->mirrorT = 0;
	m_pTexture->realWidth = 1024;
	m_pTexture->realHeight = 512;
	m_pTexture->textureBytes = m_pTexture->realWidth * m_pTexture->realHeight * 4;
	cache.cachedBytes += m_pTexture->textureBytes;
	glBindTexture( GL_TEXTURE_2D, m_pTexture->glName );
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_pTexture->realWidth, m_pTexture->realHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glBindTexture(GL_TEXTURE_2D, 0);

	ogl_glFramebufferTexture(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, m_pTexture->glName, 0);
	// check if everything is OK
	assert(checkFBO());
	ogl_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

	// Generate and initialize Pixel Buffer Objects
	glGenBuffers(2, m_aPBO);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, m_aPBO[0]);
	glBufferData(GL_PIXEL_PACK_BUFFER, 640*480*4, NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, m_aPBO[1]);
	glBufferData(GL_PIXEL_PACK_BUFFER, 640*480*4, NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}

void FrameBufferToRDRAM::Destroy() {
	ogl_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	ogl_glDeleteFramebuffers(1, &m_FBO);
	TextureCache_Remove( m_pTexture );
	glDeleteBuffers(2, m_aPBO);
}

void FrameBufferToRDRAM::CopyToRDRAM( u32 address, bool bSync ) {
	FrameBuffer *current = FrameBuffer_FindBuffer(address);
	if (current == NULL)
		return;

	address = current->startAddress;
	ogl_glBindFramebuffer(GL_READ_FRAMEBUFFER, current->fbo);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	ogl_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_FBO);
	GLuint attachment = GL_COLOR_ATTACHMENT0;
	glDrawBuffers(1, &attachment);
	ogl_glBlitFramebuffer(
		0, 0, OGL.width, OGL.height,
		0, 0, current->width, current->height,
		GL_COLOR_BUFFER_BIT, GL_LINEAR
	);
	ogl_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frameBuffer.top->fbo);

	// If Sync, read pixels from the buffer, copy them to RDRAM.
	// If not Sync, read pixels from the buffer, copy pixels from the previous buffer to RDRAM.
	if (m_aAddress[m_curIndex] == 0)
		bSync = true;
	m_curIndex = (m_curIndex + 1) % 2;
	const u32 nextIndex = bSync ? m_curIndex : (m_curIndex + 1) % 2;
	m_aAddress[m_curIndex] = address;
	glBindBuffer(GL_PIXEL_PACK_BUFFER, m_aPBO[m_curIndex]);
	ogl_glBindFramebuffer(GL_READ_FRAMEBUFFER, m_FBO);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glReadPixels( 0, 0, VI.width, VI.height, GL_RGBA, GL_UNSIGNED_BYTE, 0 );

	glBindBuffer(GL_PIXEL_PACK_BUFFER, m_aPBO[nextIndex]);
	GLubyte* pixelData = (GLubyte*)glMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
	if(pixelData == NULL) {
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
		return;
	}

	if (current->size == G_IM_SIZ_32b) {
		u32 *ptr_dst = (u32*)(RDRAM + m_aAddress[nextIndex]);
		u32 *ptr_src = (u32*)pixelData;

		for (u32 y = 0; y < VI.height; ++y) {
			for (u32 x = 0; x < VI.width; ++x)
				ptr_dst[x + y*VI.width] = ptr_src[x + (VI.height - y - 1)*VI.width];
		}
	} else {
		u16 *ptr_dst = (u16*)(RDRAM + m_aAddress[nextIndex]);
		u16 col;
		RGBA * ptr_src = (RGBA*)pixelData;

		for (u32 y = 0; y < VI.height; ++y) {
			for (u32 x = 0; x < VI.width; ++x) {
					const RGBA & c = ptr_src[x + (VI.height - y - 1)*VI.width];
					ptr_dst[(x + y*VI.width)^1] = ((c.r>>3)<<11) | ((c.g>>3)<<6) | ((c.b>>3)<<1) | (c.a == 0 ? 0 : 1);
			}
		}
	}
	glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}

void FrameBufferToRDRAM::CopyAuxBufferToRDRAM( u32 address ) {
	FrameBuffer *current = FrameBuffer_FindBuffer(address);
	if (current == NULL)
		return;

	ogl_glBindFramebuffer(GL_READ_FRAMEBUFFER, current->fbo);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	ogl_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_FBO);
	GLuint attachment = GL_COLOR_ATTACHMENT0;
	glDrawBuffers(1, &attachment);
	const u32 width  = current->width;
	const u32 height = (current->endAddress - current->startAddress + 1) / (current->width<<current->size>>1);
	ogl_glBlitFramebuffer(
		0, (current->height - height)*OGL.scaleY, width*OGL.scaleX, current->height*OGL.scaleY,
		0, 0, width, height,
		GL_COLOR_BUFFER_BIT, GL_LINEAR
	);

	char *pixelData = (char*)malloc(width * height * 4);
	if (*pixelData == NULL)
		return;

	ogl_glBindFramebuffer(GL_READ_FRAMEBUFFER, m_FBO);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glReadPixels( 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixelData );
	if (current->size == G_IM_SIZ_32b) {
		u32 *ptr_dst = (u32*)(RDRAM + address);
		u32 *ptr_src = (u32*)pixelData;

		for (u32 y = 0; y < height; ++y) {
			for (u32 x = 0; x < width; ++x) {
				const u32 c = ptr_src[x + (height - y - 1)*width];
				if (c&0xFF > 0)
					ptr_dst[x + y*width] = ptr_src[x + (height - y - 1)*width];
			}
		}
	} else {
		u16 *ptr_dst = (u16*)(RDRAM + address);
		u16 col;
		RGBA * ptr_src = (RGBA*)pixelData;

		for (u32 y = 0; y < height; ++y) {
			for (u32 x = 0; x < width; ++x) {
					const RGBA c = ptr_src[x + (height - y - 1)*width];
					if (c.a > 0)
						ptr_dst[(x + y*width)^1] = ((c.r>>3)<<11) | ((c.g>>3)<<6) | ((c.b>>3)<<1) | (c.a == 0 ? 0 : 1);
			}
		}
	}
	free( pixelData );
}

void FrameBuffer_CopyToRDRAM( u32 address, bool bSync )
{
	g_fbToRDRAM.CopyToRDRAM(address, bSync);
}

void RDRAMtoFrameBuffer::Init()
{
	m_pTexture = TextureCache_AddTop();
	m_pTexture->format = G_IM_FMT_RGBA;
	m_pTexture->clampS = 1;
	m_pTexture->clampT = 1;
	m_pTexture->frameBufferTexture = TRUE;
	m_pTexture->maskS = 0;
	m_pTexture->maskT = 0;
	m_pTexture->mirrorS = 0;
	m_pTexture->mirrorT = 0;
	m_pTexture->realWidth = 1024;
	m_pTexture->realHeight = 512;
	m_pTexture->textureBytes = m_pTexture->realWidth * m_pTexture->realHeight * 4;
	cache.cachedBytes += m_pTexture->textureBytes;
	glBindTexture( GL_TEXTURE_2D, m_pTexture->glName );
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_pTexture->realWidth, m_pTexture->realHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glBindTexture(GL_TEXTURE_2D, 0);

	// Generate Pixel Buffer Object. Initialize it later
	glGenBuffers(1, &m_PBO);
}

void RDRAMtoFrameBuffer::Destroy()
{
	TextureCache_Remove( m_pTexture );
	glDeleteBuffers(1, &m_PBO);
}

void RDRAMtoFrameBuffer::CopyFromRDRAM( u32 _address, bool _bUseAlpha)
{
	FrameBuffer *current = FrameBuffer_FindBuffer(_address);
	if (current == NULL || current->size < G_IM_SIZ_16b)
		return;

	const u32 width = current->width;
	const u32 height = current->height;
	m_pTexture->width = width;
	m_pTexture->height = height;
	const u32 dataSize = width*height*4;
	PBOBinder binder(m_PBO);
	glBufferData(GL_PIXEL_UNPACK_BUFFER, dataSize, NULL, GL_DYNAMIC_DRAW);
	GLubyte* ptr = (GLubyte*)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
	if (ptr == NULL)
		return;

	u8 * image = RDRAM + _address;
	u32 * dst = (u32*)ptr;

	u32 empty = 0;
	u32 r, g, b,a, idx;
	if (current->size == G_IM_SIZ_16b) {
		u16 * src = (u16*)image;
		u16 col;
		const u32 bound = (RDRAMSize + 1 - _address) >> 1;
		for (u32 y = 0; y < height; y++)
		{
			for (u32 x = 0; x < width; x++)
			{
				idx = (x + (height - y - 1)*width)^1;
				if (idx >= bound)
					break;
				col = src[idx];
				empty |= col;
				r = ((col >> 11)&31)<<3;
				g = ((col >> 6)&31)<<3;
				b = ((col >> 1)&31)<<3;
				a = col&1 > 0 ? 0xff : 0;
				//*(dst++) = RGBA5551_RGBA8888(c);
				dst[x + y*width] = (a<<24)|(b<<16)|(g<<8)|r;
			}
		}
	} else {
		// 32 bit
		u32 * src = (u32*)image;
		u32 col;
		const u32 bound = (RDRAMSize + 1 - _address) >> 2;
		for (u32 y=0; y < height; y++)
		{
			for (u32 x=0; x < width; x++)
			{
				idx = x + (height - y - 1)*width;
				if (idx >= bound)
					break;
				col = src[idx];
				empty |= col;
				r = (col >> 24) & 0xff;
				g = (col >> 16) & 0xff;
				b = (col >> 8) & 0xff;
				a = col & 0xff;
				dst[x + y*width] = (a<<24)|(b<<16)|(g<<8)|r;
			}
		}
	}
	glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER); // release the mapped buffer
	if (empty == 0)
		return;

	glBindTexture(GL_TEXTURE_2D, m_pTexture->glName);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, 0);
#if 0
	glBindTexture(GL_TEXTURE_2D, 0);

	ogl_glBindFramebuffer(GL_READ_FRAMEBUFFER, m_FBO);
	const GLuint attachment = GL_COLOR_ATTACHMENT0;
	glReadBuffer(attachment);
	ogl_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, current->fbo);
	glDrawBuffers(1, &attachment);
	ogl_glBlitFramebuffer(
		0, 0, width, height,
		0, 0, OGL.width, OGL.height,
		GL_COLOR_BUFFER_BIT, GL_LINEAR
		);
	ogl_glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	ogl_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frameBuffer.top->fbo);
#else
	glPushAttrib( GL_ENABLE_BIT | GL_VIEWPORT_BIT );

	glActiveTextureARB(GL_TEXTURE0_ARB);
	glEnable(GL_TEXTURE_2D);
	if (_bUseAlpha)
		Combiner_SetCombine( EncodeCombineMode( 0, 0, 0, TEXEL0, 0, 0, 0, TEXEL0, 0, 0, 0, TEXEL0, 0, 0, 0, TEXEL0 ) );
	else
		Combiner_SetCombine( EncodeCombineMode( 0, 0, 0, TEXEL0, 0, 0, 0, 1, 0, 0, 0, TEXEL0, 0, 0, 0, 1 ) );
	glEnable( GL_BLEND );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	//glDisable( GL_ALPHA_TEST );
	glDisable( GL_DEPTH_TEST );
	glDisable( GL_CULL_FACE );
	glDisable( GL_POLYGON_OFFSET_FILL );
	glDisable( GL_FOG );

	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();
	glOrtho( 0, OGL.width, 0, OGL.height, -1.0f, 1.0f );
	glViewport( 0, 0, OGL.width, OGL.height );
	glDisable( GL_SCISSOR_TEST );

	float u1, v1;

	u1 = (float)width / (float)m_pTexture->realWidth;
	v1 = (float)height / (float)m_pTexture->realHeight;

	ogl_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, current->fbo);
	const GLuint attachment = GL_COLOR_ATTACHMENT0;
	glDrawBuffers(1, &attachment);
	glBegin(GL_QUADS);
	glTexCoord2f( 0.0f, 0.0f );
	glVertex2f( 0.0f, 0.0f );

	glTexCoord2f( 0.0f, v1 );
	glVertex2f( 0.0f, (GLfloat)OGL.height );

	glTexCoord2f( u1,  v1 );
	glVertex2f( (GLfloat)OGL.width, (GLfloat)OGL.height );

	glTexCoord2f( u1, 0.0f );
	glVertex2f( (GLfloat)OGL.width, 0.0f );
	glEnd();
	ogl_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, frameBuffer.top->fbo);
	glBindTexture(GL_TEXTURE_2D, 0);

	glLoadIdentity();
	glPopAttrib();

	gSP.changed |= CHANGED_TEXTURE | CHANGED_VIEWPORT;
	gDP.changed |= CHANGED_COMBINE;
#endif
}

void FrameBuffer_CopyFromRDRAM( u32 address, bool bUseAlpha )
{
	g_RDRAMtoFB.CopyFromRDRAM(address, bUseAlpha);
}
