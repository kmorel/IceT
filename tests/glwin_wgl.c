/*
 * Copyright (C) 2003 Sandia Corporation
 * Under the terms of Contract DE-AC04-94AL85000, there is a non-exclusive
 * license for use of this work by or on behalf of the U.S. Government.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that this Notice and any statement
 * of authorship are reproduced on all copies.
 */

/* 
 * This code opens up a GL window on Windows
 * Author: Various
 * Id
 *
 */ 

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#include <stdio.h>
#include <windows.h> 
#include <GL/gl.h> 
#include <GL/glu.h> 
 

void APIENTRY	pglcErr( void );
long APIENTRY	MainWndProc( HWND, UINT, WPARAM, LPARAM ); 
BOOL APIENTRY	bSetupPixelFormat(HDC);
BOOL createRGBPalette(HDC hDC); 
void printVisualInfo(HDC hdc);

HDC   ghDC; 
HGLRC ghRC; 

int wincreat( int x, int y, int width, int height, char *title)
{ 
    MSG		msg;
    HWND	ghWnd; 
    WNDCLASS	wndclass;
    HINSTANCE hInstance = GetModuleHandle(NULL);


 
    /* Register the frame class */ 
    wndclass.style	   = CS_HREDRAW | CS_VREDRAW; 
    wndclass.lpfnWndProc   = (WNDPROC)MainWndProc; 
    wndclass.cbClsExtra	   = 0; 
    wndclass.cbWndExtra	   = 0; 
    wndclass.hInstance	   = hInstance;
    wndclass.hIcon	   = NULL;
    wndclass.hCursor	   = LoadCursor (NULL,IDC_ARROW); 
    wndclass.hbrBackground = NULL; 
    wndclass.lpszMenuName  = NULL; 
    wndclass.lpszClassName = title;
 
    if (!RegisterClass (&wndclass) ) 
    {
	printf( "register class\n" );
	fflush( stdout );
	pglcErr( );
	return FALSE; 
    }

    /* Create the frame */ 
    ghWnd = CreateWindow (title, title,
	     /*WS_POPUP | WS_MAXIMIZE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, */
	     WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
	     x, y, 
	     width, height, 
	     NULL, 
	     NULL, 
	     hInstance, 
	     NULL); 
 
    /* make sure window was created */ 

    if (!ghWnd) 
    {
	printf( "create window\n" ); fflush( stdout );
	return FALSE; 
    }

 
    /* show, enable, update main window */ 
    EnableWindow( ghWnd, TRUE );
    ShowWindow( ghWnd, SW_SHOWDEFAULT );
    UpdateWindow( ghWnd );

    /* Hide the mouse cursor.  It looks bad showing in the middle of every
       tile.*/
    ShowCursor(FALSE);


    /* 
     *	Process all pending messages 
     */ 
 
    while (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE) == TRUE) 
    { 
	    if (GetMessage(&msg, NULL, 0, 0) ) 
	    { 
		TranslateMessage(&msg); 
		DispatchMessage(&msg); 
	    } else { 
		return TRUE; 
	    } 
    } 

    return TRUE;
 
} /* end int APIENTRY pglc_wincreat( ) */
 
/* main window procedure */ 
long APIENTRY
MainWndProc( HWND hWnd, UINT uMsg, WPARAM  wParam, LPARAM  lParam ) 
{ 
    
    LONG    lRet = 1; 
 
    switch (uMsg) {
 
    case WM_CREATE: 
	ghDC = GetDC(hWnd);

	if (!bSetupPixelFormat(ghDC))
    {
      printf( "bSetupPixelFormat( ) failed\n" );
	  PostQuitMessage (0); 
    }
	ghRC = wglCreateContext(ghDC);
	wglMakeCurrent(ghDC, ghRC); 
	break; 
 
    case WM_PAINT: 
	ValidateRect(hWnd,NULL);
	break; 
 
    case WM_SIZE: 
	break; 
 
    case WM_CLOSE: 
	if (ghRC) 
	    wglDeleteContext(ghRC); 
	if (ghDC) 
	    ReleaseDC(hWnd, ghDC); 
	ghRC = 0; 
	ghDC = 0; 
 
	DestroyWindow (hWnd); 
	break; 
 
    case WM_DESTROY: 
	if (ghRC) 
	    wglDeleteContext(ghRC); 
	if (ghDC) 
	    ReleaseDC(hWnd, ghDC); 
 
	PostQuitMessage (0); 
	break; 
     
    default: 
	lRet = DefWindowProc (hWnd, uMsg, wParam, lParam); 
	break; 
    } 
 
    return lRet; 
} /* end long APIENTRY MainWndProc( ) */
 
BOOL APIENTRY
bSetupPixelFormat(HDC hdc) 
{ 
    PIXELFORMATDESCRIPTOR pfd, *ppfd; 
    int pixelformat; 
 
    ppfd = &pfd; 
 
    ppfd->nSize = sizeof(PIXELFORMATDESCRIPTOR); 
    ppfd->nVersion = 1; 
    ppfd->dwFlags = PFD_DRAW_TO_WINDOW
		  | PFD_SUPPORT_OPENGL
		  | PFD_DOUBLEBUFFER;
    ppfd->iPixelType = PFD_TYPE_RGBA;  
    ppfd->cRedBits = 4;
    ppfd->cGreenBits = 4;
    ppfd->cBlueBits = 4;
    ppfd->cAlphaBits = 4;
    ppfd->cDepthBits = 16; 
    ppfd->cAccumBits = 0; 
    ppfd->cStencilBits = 0; 
    ppfd->iLayerType = PFD_MAIN_PLANE;
 
    /* Give my requirements and then settle for what */
    /* it gives back */
    pixelformat = ChoosePixelFormat(hdc, ppfd);
    SetPixelFormat(hdc, pixelformat, ppfd);

    /* I need to create a palette if in 8bpp mode */
    createRGBPalette(hdc);

    /* Now print out info about the visual I got */
    //printVisualInfo(hdc);
 
    return TRUE; 
} /* end BOOL bSetupPixelFormat(HDC hdc) */

void APIENTRY
pglcErr( void )
{

  LPVOID lpMsgBuf;
  FormatMessage( 
      FORMAT_MESSAGE_ALLOCATE_BUFFER | 
      FORMAT_MESSAGE_FROM_SYSTEM | 
      FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL,
      GetLastError(),
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
      (LPTSTR) &lpMsgBuf,
      0,
      NULL 
  );
  // Process any inserts in lpMsgBuf.
  // ...
  // Display the string.
  MessageBox( NULL, (LPCTSTR)lpMsgBuf, "Error",
	      MB_OK | MB_ICONINFORMATION );
  // Free the buffer.
  LocalFree( lpMsgBuf );

}   /* end void pglcErr( void ) */


void printVisualInfo(HDC hdc) {

    PIXELFORMATDESCRIPTOR pfd, *ppfd; 
    int pixelformat; 
    ppfd = &pfd; 

    /* get the current pixel format index */ 
    pixelformat = GetPixelFormat(hdc); 
 
    /* obtain a detailed description of that pixel format */ 
    DescribePixelFormat(hdc, pixelformat, 
	sizeof(PIXELFORMATDESCRIPTOR), ppfd);
    
    /* Output pixel format */
    printf("------Pixel Format-------\n");
    if (ppfd->dwFlags&PFD_DOUBLEBUFFER)		printf("PFD_DOUBLEBUFFER\n");
    if (ppfd->dwFlags&PFD_STEREO)		printf("PFD_STEREO\n");
    if (ppfd->dwFlags&PFD_DRAW_TO_WINDOW)	printf("PFD_DRAW_TO_WINDOW\n");
    if (ppfd->dwFlags&PFD_DRAW_TO_BITMAP)	printf("PFD_DRAW_TO_BITMAP\n");
    if (ppfd->dwFlags&PFD_SUPPORT_GDI)		printf("PFD_SUPPORT_GDI\n");
    if (ppfd->dwFlags&PFD_SUPPORT_OPENGL)	printf("PFD_SUPPORT_OPENGL\n");
    if (ppfd->dwFlags&PFD_GENERIC_FORMAT)	printf("PFD_GENERIC_FORMAT\n");
    if (ppfd->dwFlags&PFD_NEED_PALETTE)		printf("PFD_NEED_PALETTE\n");
    if (ppfd->dwFlags&PFD_NEED_SYSTEM_PALETTE)	printf("PFD_NEED_SYSTEM_PALETTE\n");
    if (ppfd->dwFlags&PFD_SWAP_EXCHANGE)	printf("PFD_SWAP_EXCHANGE\n");
    if (ppfd->dwFlags&PFD_SWAP_COPY)		printf("PFD_SWAP_COPY\n");
    if (ppfd->dwFlags&PFD_SWAP_LAYER_BUFFERS)	printf("PFD_SWAP_LAYER_BUFFERS\n");
    if (ppfd->dwFlags&PFD_GENERIC_ACCELERATED)	printf("PFD_GENERIC_ACCELERATED\n");
    if (ppfd->dwFlags&PFD_SUPPORT_DIRECTDRAW)	printf("PFD_SUPPORT_DIRECTDRAW\n");
    printf("------Pixel Type-------\n");
    if (ppfd->iPixelType==0)	printf("PFD_TYPE_RGBA\n");
    if (ppfd->iPixelType==1)	printf("PFD_TYPE_COLORINDEX\n");
    printf("------Layer Type-------\n");
    if (ppfd->iLayerType==0)	printf("PFD_MAIN_PLANE\n");
    if (ppfd->iLayerType==1)	printf("PFD_OVERLAY_PLANE\n");
    if (ppfd->iLayerType==-1)	printf("PFD_UNDERLAY_PLANE\n");
    printf("ColorBits: %d\n",ppfd->cColorBits);
    printf("DepthBits: %d\n",ppfd->cDepthBits);
}

/* OpenGL 332 palette */
unsigned char m_threeto8[8] = {0, 0111>>1, 0222>>1, 0333>>1, 0444>>1, 0555>>1, 0666>>1, 0377};
unsigned char m_twoto8[4] = { 0, 0x55, 0xaa, 0xff};
unsigned char m_oneto8[2] = {0, 255};
int m_defaultOverride[13] = {0, 3, 24, 27, 64, 67, 88, 173, 181, 236, 247, 164, 91};
PALETTEENTRY m_defaultPalEntry[20] = {
    { 0,   0,	0,    0 }, 
    { 0x80,0,	0,    0 }, 
    { 0,   0x80,0,    0 }, 
    { 0x80,0x80,0,    0 }, 
    { 0,   0,	0x80, 0 },
    { 0x80,0,	0x80, 0 },
    { 0,   0x80,0x80, 0 },
    { 0xC0,0xC0,0xC0, 0 }, 

    { 192, 220, 192,  0 }, 
    { 166, 202, 240,  0 },
    { 255, 251, 240,  0 },
    { 160, 160, 164,  0 }, 

    { 0x80,0x80,0x80, 0 }, 
    { 0xFF,0,	0,    0 },
    { 0,   0xFF,0,    0 },
    { 0xFF,0xFF,0,    0 },
    { 0,   0,	0xFF, 0 },
    { 0xFF,0,	0xFF, 0 },
    { 0,   0xFF,0xFF, 0 },
    { 0xFF,0xFF,0xFF, 0 }  
  };
/*
 * ComponentFromIndex
 */
unsigned char ComponentFromIndex(int i, UINT nbits, UINT shift)
{
    unsigned char val = (unsigned char) (i >> shift);

    switch (nbits) {
    case 1:
	val &= 0x1;
	return m_oneto8[val];
	break ;
    case 2:
	val &= 0x3;
	return m_twoto8[val];
	break ;
    case 3:
	val &= 0x7;
	return m_threeto8[val];
	break ;
    default:
	return 0;
    }
}

BOOL createRGBPalette(HDC hDC)
{
  
   PIXELFORMATDESCRIPTOR pfd;
   LOGPALETTE* pPal;
   HPALETTE hPal;
   int i,j,n;

   /* Check to see if we need a palette */
   n = GetPixelFormat(hDC);
   DescribePixelFormat(hDC, n, sizeof(PIXELFORMATDESCRIPTOR), &pfd);
   if (!(pfd.dwFlags & PFD_NEED_PALETTE)) return FALSE ; 

   /* Allocate a log pal and fill it with the color table info */
   pPal = (LOGPALETTE*) malloc(sizeof(LOGPALETTE) + 256 * sizeof(PALETTEENTRY));
   pPal->palVersion = 0x300; /* Windows 3.0 */
   pPal->palNumEntries = 256; /* table size */

   /*/ Create palette */
   n = 1 << pfd.cColorBits;
   for (i=0; i<n; i++)
   {
      pPal->palPalEntry[i].peRed =
	     ComponentFromIndex(i, pfd.cRedBits, pfd.cRedShift);
      pPal->palPalEntry[i].peGreen =
	     ComponentFromIndex(i, pfd.cGreenBits, pfd.cGreenShift);
      pPal->palPalEntry[i].peBlue =
	     ComponentFromIndex(i, pfd.cBlueBits, pfd.cBlueShift);
      pPal->palPalEntry[i].peFlags = 0;
   }

   if ((pfd.cColorBits == 8)			       &&
       (pfd.cRedBits   == 3) && (pfd.cRedShift	 == 0) &&
       (pfd.cGreenBits == 3) && (pfd.cGreenShift == 3) &&
       (pfd.cBlueBits  == 2) && (pfd.cBlueShift	 == 6))
   {
      for (j = 1 ; j <= 12 ; j++)
	  pPal->palPalEntry[m_defaultOverride[j]] = m_defaultPalEntry[j];
   }


   hPal = CreatePalette(pPal);
   SelectPalette(hDC,hPal,FALSE);
   

   return TRUE;
}

void swap_buffers(void)
{
    SwapBuffers(ghDC);
}

#endif /*WIN32*/

#ifdef __cplusplus
}
#endif
