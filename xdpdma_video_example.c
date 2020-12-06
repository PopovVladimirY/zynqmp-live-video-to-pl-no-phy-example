/*******************************************************************************
 *
 * Copyright (C) 2017 Xilinx, Inc.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 *
 *
*******************************************************************************/
/*****************************************************************************/
/**
*
* @file xdpdma_video_example.c
*
*
* This file contains a design example using the DPDMA driver (XDpDma)
* This example demonstrates the use of DPDMA for displaying a Graphics Overlay
*
* @note
*
* None.
*
* <pre>
* MODIFICATION HISTORY:
*
* Ver   Who Date     Changes
* ----- --- -------- -----------------------------------------------
* 1.0	aad 10/19/17	Initial Release
* 1.1   aad 02/22/18    Fixed the header
* 2.0   vpo 01/12/20    Implementing Live Video output to FPGA support
*                       with not PHY connected to Display Port. 
*                       Fixing STRIDE calculation and DPDMA configuration.
*</pre>
*
******************************************************************************/

/***************************** Include Files *********************************/

#include "xil_exception.h"
#include "xil_printf.h"
#include "xil_cache.h"
#include "xdpdma_video_example.h"

/************************** Constant Definitions *****************************/
#define DPPSU_DEVICE_ID		XPAR_PSU_DP_DEVICE_ID
#define AVBUF_DEVICE_ID		XPAR_PSU_DP_DEVICE_ID
#define DPDMA_DEVICE_ID		XPAR_XDPDMA_0_DEVICE_ID
#define DPPSU_INTR_ID		151
#define DPDMA_INTR_ID		154
#define INTC_DEVICE_ID		XPAR_SCUGIC_0_DEVICE_ID

#define DPPSU_BASEADDR		XPAR_PSU_DP_BASEADDR
#define AVBUF_BASEADDR		XPAR_PSU_DP_BASEADDR
#define DPDMA_BASEADDR		XPAR_PSU_DPDMA_BASEADDR

#define FRAME_WIDTH			800 //1920
#define FRAME_HEIGHT		480 //1080

#define VSIZE			    (FRAME_HEIGHT)			/* HTotal * BPP */
#define LINESIZE			(FRAME_WIDTH * 4)		/* HTotal * BPP for RGBA*/
#define STRIDE				(((LINESIZE + 255) / 256) * 256) 	/* The stride value should be aligned to 256*/
#define TOTALPIXEL			(LINESIZE * VSIZE)		/* HTotal * VTotal * BPP */
#define BUFFERSIZE			(STRIDE * VSIZE)		/* HTotal * VTotal * BPP */

/************************** Variable Declarations ***************************/
u8 Frame[BUFFERSIZE] __attribute__ ((__aligned__(256)));
u8 Frame2[BUFFERSIZE] __attribute__ ((__aligned__(256)));
XDpDma_FrameBuffer FrameBuffer;
XDpDma_FrameBuffer FrameBuffer2;

u8 *DrawSquare(u8* Frame);

/**************************** Type Definitions *******************************/

/*****************************************************************************/
/**
*
* Main function to call the DPDMA Video example.
*
* @param	None
*
* @return	XST_SUCCESS if successful, otherwise XST_FAILURE.
*
* @note		None
*
******************************************************************************/
int main()
{
	int Status;

	Xil_DCacheDisable();
	Xil_ICacheDisable();

	/*
	 * Following lines are an example implementation of a custom LCD interface
	 * module in FPGA which will be receiving the Live Video from Display Port.
	*/
#define CUSTOM_LCD
#ifdef CUSTOM_LCD
	/* Check FPGA version */
	uint32_t ver = Xil_In32(0x80000000);
	xil_printf("FPGA version: 0x%08X \r\n", ver);
	/* Enable LCD */
	Xil_Out32(0x8000C004, 0x111);
#endif

	xil_printf("DPDMA Generic Video Example Test \r\n");
	xil_printf("Video : line = %d, stride = %d, frame = %d \r\n", LINESIZE, STRIDE, BUFFERSIZE);

	Status = DpdmaVideoExample(&RunCfg);
	if (Status != XST_SUCCESS) {
		xil_printf("DPDMA Video Example Test Failed\r\n");
		return XST_FAILURE;
	}

#ifdef NO_PHY
	DpPsu_Run(&RunCfg);
#endif

	/*
	*  IMPORTANT: The application shall not exit for the example to work
	*  properly. 
	*/

	/*
	 * Draw bouncing square using two frame buffers for flip-flop
	 */
	uint32_t cnt = 0;
	while(1)
	{
		if (cnt & 1)
		{
			DrawSquare(Frame);
			XDpDma_DisplayGfxFrameBuffer(RunCfg.DpDmaPtr, &FrameBuffer);
		}
		else
		{
			DrawSquare(Frame2);
			XDpDma_DisplayGfxFrameBuffer(RunCfg.DpDmaPtr, &FrameBuffer2);
		}

		usleep(33333);

		cnt++;
	}

    return XST_SUCCESS;
}

/*****************************************************************************/
/**
*
* The purpose of this function is to illustrate how to use the XDpDma device
* driver in Graphics overlay mode.
*
* @param	RunCfgPtr is a pointer to the application configuration structure.
*
* @return	XST_SUCCESS if successful, else XST_FAILURE.
*
* @note		None.
*
*****************************************************************************/
int DpdmaVideoExample(Run_Config *RunCfgPtr)

{
	u32 Status;
	/* Initialize the application configuration */
	InitRunConfig(RunCfgPtr);
	Status = InitDpDmaSubsystem(RunCfgPtr);
	if (Status != XST_SUCCESS) {
				return XST_FAILURE;
	}

	xil_printf("Generating Overlay.....\n\r");
	GraphicsOverlay(Frame, RunCfgPtr, 0x0FF000FF, 0xF0FF0000);
	GraphicsOverlay(Frame2, RunCfgPtr, 0xF0FF0000, 0x0FF000FF);

	/* Populate the FrameBuffer structure with the frame attributes */
	FrameBuffer.Address = (INTPTR)Frame;
	FrameBuffer.Stride = STRIDE;
	FrameBuffer.LineSize = LINESIZE;
	FrameBuffer.Size = TOTALPIXEL;

	FrameBuffer2.Address = (INTPTR)Frame2;
	FrameBuffer2.Stride = STRIDE;
	FrameBuffer2.LineSize = LINESIZE;
	FrameBuffer2.Size = TOTALPIXEL;

	SetupInterrupts(RunCfgPtr);

	return XST_SUCCESS;
}

/*****************************************************************************/
/**
*
* The purpose of this function is to initialize the application configuration.
*
* @param	RunCfgPtr is a pointer to the application configuration structure.
*
* @return	None.
*
* @note		None.
*
*****************************************************************************/
void InitRunConfig(Run_Config *RunCfgPtr)
{
	/* Initial configuration parameters. */
		RunCfgPtr->DpPsuPtr   = &DpPsu;
		RunCfgPtr->IntrPtr   = &Intr;
		RunCfgPtr->AVBufPtr  = &AVBuf;
		RunCfgPtr->DpDmaPtr  = &DpDma;
		RunCfgPtr->VideoMode = XVIDC_VM_800x480_60_P;
//		RunCfgPtr->VideoMode = XVIDC_VM_1920x1080_60_P;
		RunCfgPtr->Bpc		 = XVIDC_BPC_8;
		RunCfgPtr->ColorEncode			= XDPPSU_CENC_RGB;
		RunCfgPtr->UseMaxCfgCaps		= 1;
		RunCfgPtr->LaneCount			= LANE_COUNT_2;
		RunCfgPtr->LinkRate				= LINK_RATE_540GBPS;
		RunCfgPtr->EnSynchClkMode		= 0;
		RunCfgPtr->UseMaxLaneCount		= 1;
		RunCfgPtr->UseMaxLinkRate		= 1;
}

/*****************************************************************************/
/**
*
* The purpose of this function is to initialize the DP Subsystem (XDpDma,
* XAVBuf, XDpPsu)
*
* @param	RunCfgPtr is a pointer to the application configuration structure.
*
* @return	None.
*
* @note		None.
*
*****************************************************************************/
int InitDpDmaSubsystem(Run_Config *RunCfgPtr)
{
	u32 Status;
	XDpPsu		*DpPsuPtr = RunCfgPtr->DpPsuPtr;
	XDpPsu_Config	*DpPsuCfgPtr;
	XAVBuf		*AVBufPtr = RunCfgPtr->AVBufPtr;
	XDpDma_Config *DpDmaCfgPtr;
	XDpDma		*DpDmaPtr = RunCfgPtr->DpDmaPtr;


	/* Initialize DisplayPort driver. */
	DpPsuCfgPtr = XDpPsu_LookupConfig(DPPSU_DEVICE_ID);
	XDpPsu_CfgInitialize(DpPsuPtr, DpPsuCfgPtr, DpPsuCfgPtr->BaseAddr);
	/* Initialize Video Pipeline driver */
	XAVBuf_CfgInitialize(AVBufPtr, DpPsuPtr->Config.BaseAddr, AVBUF_DEVICE_ID);

	/* Initialize the DPDMA driver */
	DpDmaCfgPtr = XDpDma_LookupConfig(DPDMA_DEVICE_ID);
	XDpDma_CfgInitialize(DpDmaPtr,DpDmaCfgPtr);

	/* Initialize the DisplayPort TX core. */
	Status = XDpPsu_InitializeTx(DpPsuPtr);
#ifndef NO_PHY	
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}
#endif
	/* Set the format graphics frame for DPDMA*/
	Status = XDpDma_SetGraphicsFormat(DpDmaPtr, RGBA8888);
	if (Status != XST_SUCCESS) {
			return XST_FAILURE;
	}
	/* Set the format graphics frame for Video Pipeline*/
	Status = XAVBuf_SetInputNonLiveGraphicsFormat(AVBufPtr, RGBA8888);
	if (Status != XST_SUCCESS) {
			return XST_FAILURE;
	}
	/* Set the QOS for Video */
	XDpDma_SetQOS(RunCfgPtr->DpDmaPtr, 11);
	/* Enable the Buffers required by Graphics Channel */
	XAVBuf_EnableGraphicsBuffers(RunCfgPtr->AVBufPtr, 1);
	/* Set the output Video Format */
	XAVBuf_SetOutputVideoFormat(AVBufPtr, RGB_8BPC);

	/* Select the Input Video Sources.
	 * Here in this example we are going to demonstrate
	 * graphics overlay over the TPG video.
	 */
	XAVBuf_InputVideoSelect(AVBufPtr,

	/* Select live video input option
	 * XAVBUF_VIDSTREAM1_NONE
	 * XAVBUF_VIDSTREAM1_LIVE
	 * XAVBUF_VIDSTREAM1_TPG
	*/
							XAVBUF_VIDSTREAM1_TPG,
							XAVBUF_VIDSTREAM2_NONLIVE_GFX);

	/* Configure Video pipeline for graphics channel */
	XAVBuf_ConfigureGraphicsPipeline(AVBufPtr);
	/* Configure the output video pipeline */
	XAVBuf_ConfigureOutputVideo(AVBufPtr);
	/* Disable the global alpha, since we are using the pixel based alpha */
	XAVBuf_SetBlenderAlpha(AVBufPtr, 0, 0);
	/* Set the clock mode */
	XDpPsu_CfgMsaEnSynchClkMode(DpPsuPtr, RunCfgPtr->EnSynchClkMode);
	/* Set the clock source depending on the use case.
	 * Here for simplicity we are using PS clock as the source*/

	/* Actual clock selection depends on how FPGA will treat Live Video.
	 * Select correct option for your design.
	 * Original example uses XAVBUF_PS_CLK for video and audio
	*/
#ifndef NO_PHY
	XAVBuf_SetAudioVideoClkSrc(AVBufPtr, XAVBUF_PS_CLK, XAVBUF_PS_CLK);
#else
	XAVBuf_SetAudioVideoClkSrc(AVBufPtr, XAVBUF_PL_CLK, XAVBUF_PS_CLK);
#endif	

	/* Issue a soft reset after selecting the input clock sources */
	XAVBuf_SoftReset(AVBufPtr);

	return XST_SUCCESS;
}

/*****************************************************************************/
/**
*
* The purpose of this function is to setup call back functions for the DP
* controller interrupts.
*
* @param	RunCfgPtr is a pointer to the application configuration structure.
*
* @return	None.
*
* @note		None.
*
*****************************************************************************/
void SetupInterrupts(Run_Config *RunCfgPtr)
{
	XDpPsu *DpPsuPtr = RunCfgPtr->DpPsuPtr;
	XScuGic		*IntrPtr = RunCfgPtr->IntrPtr;
	XScuGic_Config	*IntrCfgPtr;
	u32  IntrMask = XDPPSU_INTR_HPD_IRQ_MASK | XDPPSU_INTR_HPD_EVENT_MASK;

	XDpPsu_WriteReg(DpPsuPtr->Config.BaseAddr, XDPPSU_INTR_DIS, 0xFFFFFFFF);
	XDpPsu_WriteReg(DpPsuPtr->Config.BaseAddr, XDPPSU_INTR_MASK, 0xFFFFFFFF);

	XDpPsu_SetHpdEventHandler(DpPsuPtr, DpPsu_IsrHpdEvent, RunCfgPtr);
	XDpPsu_SetHpdPulseHandler(DpPsuPtr, DpPsu_IsrHpdPulse, RunCfgPtr);

	/* Initialize interrupt controller driver. */
	IntrCfgPtr = XScuGic_LookupConfig(INTC_DEVICE_ID);
	XScuGic_CfgInitialize(IntrPtr, IntrCfgPtr, IntrCfgPtr->CpuBaseAddress);

	/* Register ISRs. */
	XScuGic_Connect(IntrPtr, DPPSU_INTR_ID,
			(Xil_InterruptHandler)XDpPsu_HpdInterruptHandler, RunCfgPtr->DpPsuPtr);

	/* Trigger DP interrupts on rising edge. */
	XScuGic_SetPriorityTriggerType(IntrPtr, DPPSU_INTR_ID, 0x0, 0x03);


	/* Connect DPDMA Interrupt */
	XScuGic_Connect(IntrPtr, DPDMA_INTR_ID,
			(Xil_ExceptionHandler)XDpDma_InterruptHandler, RunCfgPtr->DpDmaPtr);

	/* Initialize exceptions. */
	Xil_ExceptionInit();
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_IRQ_INT,
			(Xil_ExceptionHandler)XScuGic_DeviceInterruptHandler,
			INTC_DEVICE_ID);

	/* Enable exceptions for interrupts. */
	Xil_ExceptionEnableMask(XIL_EXCEPTION_IRQ);
	Xil_ExceptionEnable();

	/* Enable DP interrupts. */
	XScuGic_Enable(IntrPtr, DPPSU_INTR_ID);
	XDpPsu_WriteReg(DpPsuPtr->Config.BaseAddr, XDPPSU_INTR_EN, IntrMask);

	/* Enable DPDMA Interrupts */
	XScuGic_Enable(IntrPtr, DPDMA_INTR_ID);
	XDpDma_InterruptEnable(RunCfgPtr->DpDmaPtr, XDPDMA_IEN_VSYNC_INT_MASK);

}
/*****************************************************************************/
/**
*
* The purpose of this function is to generate a Graphics frame of the format
* RGBA8888 which generates an overlay on 1/2 of the bottom of the screen.
* This is just to illustrate the functionality of the graphics overlay.
*
* @param	RunCfgPtr is a pointer to the application configuration structure.
* @param	Frame is a pointer to a buffer which is going to be populated with
* 			rendered frame
*
* @return	Returns a pointer to the frame.
*
* @note		None.
*
*****************************************************************************/
u8 *GraphicsOverlay(u8* Frame, Run_Config *RunCfgPtr, uint32_t color1, uint32_t color2)
{
	u64 Index;
	u32 *RGBA;
	RGBA = (u32 *) Frame;
	/*
		 * Red at the top half
		 * Alpha = 0x0F
		 * */
	for(Index = 0; Index < (BUFFERSIZE/4) /2; Index ++) {
		/*
		 * Green at the bottom half
		 * Alpha = 0xF0
		 * */
		RGBA[Index] = color1;//0xF0FF0000;
	}
	for(; Index < BUFFERSIZE/4; Index ++) {
		RGBA[Index] = color2;//0x0FF000FF;
	}
	return Frame;
}

u8 *DrawSquare(u8* Frame)
{
	static int32_t x = 0;
	static int32_t y = 0;
	static int32_t w = 80;
	static int32_t h = 80;
	static uint32_t color = 0x00FFFFFF;
	static uint32_t alpha = 0x80;
	static uint32_t ca = 1;

	static int32_t cx = 3;
	static int32_t cy = 5;

	static int32_t sx = LINESIZE / 4;
	static int32_t sy = VSIZE;

	//
	u32 *RGBA = (u32 *) Frame;

	// Alpha 0.

	uint32_t clr = (color & 0xFFFFFF) | (alpha << 24);

	color++;

	alpha += ca;

	if (alpha == 0xFF || alpha == 0x80)
	{
		ca = -ca;
	}

	memset(RGBA, 0, BUFFERSIZE);

	u32* ptr = RGBA + x + y*STRIDE/4;
	for (int i = 0; i < h; i++)
	{
		for (int j = 0; j < w; j++)
		{
			*ptr = clr;
			ptr++;
		}
		ptr += STRIDE/4 - w;
	}


	// update position for the next frame
	if (cx > 0)
	{
		// check right boundary
		if (x + w + cx > sx - 1)
		{
			cx = -cx;
		}
	}
	else
	{
		// check left boundary
		if (x + cx < 0)
		{
			cx = -cx;
		}
	}

	if (cy > 0)
	{
		// check bottom boundary
		if (y + h + cy > sy - 1)
		{
			cy = -cy;
		}
	}
	else
	{
		// check top boundary
		if (y + cy < 0)
		{
			cy = -cy;
		}
	}

	x += cx;
	y += cy;

	return Frame;
}
