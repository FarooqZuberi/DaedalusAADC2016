/**
Copyright (c) 
Audi Autonomous Driving Cup. All rights reserved. TEAM DAEDALUS
 
Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 
1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
3.  All advertising materials mentioning features or use of this software must display the following acknowledgement: �This product includes software developed by the Audi AG and its contributors for Audi Autonomous Driving Cup.�
4.  Neither the name of Audi nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
 
THIS SOFTWARE IS PROVIDED BY AUDI AG AND CONTRIBUTORS �AS IS� AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL AUDI AG OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


Summary:

1) Filter for performing action on the crossroads.

**********************************************************************
* $Author:: spiesra $  $Date:: 2015-05-13 08:29:07#$ $Rev:: 35003   $
* Updated By:: Amanullah Tariq 
**********************************************************************/

#include "stdafx.h"
#include "DAEDALUS_util.h"
#include "daedalus_action_enums.h"
#include "daedalus_action_constant.h"
#include "constant.h"
#include "IPM.h"
#include "ParkingType.h"
#include "daedalus_action_db.h"

ADTF_FILTER_PLUGIN("DAEDALUS Action Distance Filter", OID_ADTF_DaedalusActionDistanceFilter,cActionDistanceFilter )

const int width = 640;
const int height = 480;
const float scale = 1.0;
static int imageCount = 0;


namespace patch
{
	template < typename T > std::string to_string(const T& n)
	{
		std::ostringstream stm;
		stm << n;
		return stm.str();
	}
}

cActionDistanceFilter::cActionDistanceFilter(const tChar* __info) : adtf::cFilter(__info)
{

	/****************** Start of Debug ***************/
	SetPropertyBool(AF_PROP_SHOW_LOG,tTrue);
	SetPropertyStr(AF_PROP_SHOW_LOG NSSUBPROP_DESCRIPTION, "If true show log values");
	/***************** End of Debug *****************/

	/****************** Select Type of Parking ***************/
	SetPropertyInt(AF_PROP_LAST_COMMAND,-1);
	SetPropertyStr(AF_PROP_LAST_COMMAND NSSUBPROP_DESCRIPTION, "Number of last parking type");
	/***************** End of Debug *****************/

	SetChangeLaneProperties();
	SetCrossParkingProperties();
	SetParallelParkingProperties();
	SetPullOverLeftProperties();
	SetPullOverRightProperties();
	SetPullOverLeftFromParallelParkingProperties();
	SetPullOverRightFromParallelParkingProperties();

	/*************************Start Properties for Left and right turn of the car **********************************/
	SetLeftTurnProperties();
	SetRightTurnProperties();
	SetStraightProperties();


	/*************************End Properties for Left and right turn of the car **********************************/
}

cActionDistanceFilter::~cActionDistanceFilter()
{

}

tResult cActionDistanceFilter::Init(tInitStage eStage, __exception)
{
    RETURN_IF_FAILED(cFilter::Init(eStage, __exception_ptr));

    if (eStage == StageFirst)
    {
		CreateInputPins(__exception_ptr);
		CreateOutputPins(__exception_ptr);
    }
    else if (eStage == StageNormal)
    {
		isObjectRight = false;
		isObjectBack = false;
		isObjectFront = false;
		isObjectLeft = false;
        startLaneTracking = false;
		m_ui8InitCtrl = 0;
		pp_counter = 0;
		cp_counter = 0;
		total_counter = 51;
		m_bFirstFrame = true;
		DriverNextAction = -1;
		prevDriverAction = -1;
		DriverModuleBufferReaderFlag = tFalse;
		WheelEncoderModuleBufferReaderFlag = tFalse;
		ObstacleSensorModuleBufferReaderFlag = tFalse;
		ReadProperties();
		counter_ = 0;
		active_ = false;
		firstTime = false;
		OverallDistance = 0.0;
		isObjectDetected = false;
		WheelEncoderDistance = 0.0;
    }
    else if (eStage == StageGraphReady)
    {
    }

    RETURN_NOERROR;
}

tResult cActionDistanceFilter::OnPinEvent(IPin* pSource, tInt nEventCode, tInt nParam1, tInt nParam2, IMediaSample* pMediaSample)
{
    
    RETURN_IF_POINTER_NULL(pMediaSample);
    RETURN_IF_POINTER_NULL(pSource);
    if(nEventCode == IPinEventSink::PE_MediaSampleReceived)
    {
        tTimeStamp InputTimeStamp;
        InputTimeStamp = pMediaSample->GetTime();


		 if(pSource == &m_iVideoInputPin)
        {
            //Videoformat
            if (m_bFirstFrame)
            {        
                cObjectPtr<IMediaType> pType;
                RETURN_IF_FAILED(m_iVideoInputPin.GetMediaType(&pType));
                cObjectPtr<IMediaTypeVideo> pTypeVideo;
                RETURN_IF_FAILED(pType->GetInterface(IID_ADTF_MEDIA_TYPE_VIDEO, (tVoid**)&pTypeVideo));
                const tBitmapFormat* pFormat = pTypeVideo->GetFormat();                                
                if (pFormat == NULL)
                {
                    LOG_ERROR("No Bitmap information found on pin \"input\"");
                    RETURN_ERROR(ERR_NOT_SUPPORTED);
                }
                m_sInputFormat.nPixelFormat = pFormat->nPixelFormat;
                m_sInputFormat.nWidth = pFormat->nWidth;
                m_sInputFormat.nHeight =  pFormat->nHeight;
                m_sInputFormat.nBitsPerPixel = pFormat->nBitsPerPixel;
                m_sInputFormat.nBytesPerLine = pFormat->nBytesPerLine;
                m_sInputFormat.nSize = pFormat->nSize;
                m_sInputFormat.nPaletteSize = pFormat->nPaletteSize;
                m_bFirstFrame = false;
            }
            ProcessInput(pMediaSample, InputTimeStamp);
        }
        else if(pSource == &m_iDriverAction)
        {
            tUInt32 DriverActionTime = 0;
            tBool DriverIsProcessed = tFalse;
            {   // focus for sample read lock
                    __adtf_sample_read_lock_mediadescription(m_pDescriptionDecisionInput,pMediaSample,pCoderInput);       
                    if(!DriverModuleBufferReaderFlag)
                    {
                        pCoderInput->GetID("i32Value", DriverNextActionBuffer);
                        pCoderInput->GetID("ui32Timestamp",DriverTimeBuffer);
                        pCoderInput->GetID("bIsExecuted",DriverIsProcessedBuffer);
                        DriverModuleBufferReaderFlag = tTrue;
                    }  
                    // get the values from DriverNextAction
                    pCoderInput->Get(DriverNextActionBuffer, (tVoid*)&DriverNextAction);
                    pCoderInput->Get(DriverTimeBuffer, (tVoid*)&DriverActionTime);
                    pCoderInput->Get(DriverIsProcessedBuffer, (tVoid*)&DriverIsProcessed);       
			}
            if(DriverNextAction != -1)
            {
                active_ = true;
			    firstTime = true;
                TransmitStartLaneTracking(tFalse,m_tsArduinoTime);
                startLaneTracking = true;
              
            }
			
			if (m_bShowLog)   
			{
                    LOG_ERROR(cString::Format("Action Fiter: DriverModule Data: ID -> %d time: %d isProcessed: %d",DriverNextAction, DriverActionTime,DriverIsProcessed));
			}
        }
		else if(pSource == &m_iWheelEncoderDistance)
		{
			if(active_)
            {  
				
				tUInt32 WheelEncoderTime = 0;
				// focus for sample read lock
                // read-out the incoming Media Sample
                __adtf_sample_read_lock_mediadescription(m_pDescriptionOutputSampleDistance,pMediaSample,pCoderInput);       
                // get IDs
                if(!WheelEncoderModuleBufferReaderFlag)
                {
                    pCoderInput->GetID("f32Value", WheelEncoderDistanceBuffer);
                    pCoderInput->GetID("ui32ArduinoTimestamp",WheelEncoderDistanceTimeBuffer);
                    WheelEncoderModuleBufferReaderFlag = tTrue;
                }  
                // get the values from DriverNextAction
                pCoderInput->Get(WheelEncoderDistanceBuffer, (tVoid*)&WheelEncoderDistance);
                pCoderInput->Get(WheelEncoderDistanceTimeBuffer, (tVoid*)&WheelEncoderTime);   
            

				if (m_bShowLog)   
				{
						LOG_ERROR(cString::Format("Action Fiter: WheelEncoderDistance Data: Dist -> %f time: %d" ,WheelEncoderDistance, WheelEncoderTime));
				}

			}
			
			CustomCycle();
		}
		else if(pSource == &SensorInputPin)
		{
			if(active_)
			{
				tFloat32 us_front = 0.0f;
				tFloat32 us_front_left = 0.0f;
				tFloat32 us_front_right = 0.0f;
				tFloat32 us_front_back = 0.0f;

				{   
					__adtf_sample_read_lock_mediadescription(m_pDescriptionOutputSensor,pMediaSample,pCoderInput);       
					// get IDs
					if(!ObstacleSensorModuleBufferReaderFlag)
					{
						pCoderInput->GetID("minimum_dist[0]", SensorInputFrontBuffer);
						pCoderInput->GetID("minimum_dist[1]", SensorInputFrontLeftBuffer);
						pCoderInput->GetID("minimum_dist[2]", SensorInputFrontRightBuffer);
						pCoderInput->GetID("minimum_dist[3]", SensorInputFrontBackBuffer);
						ObstacleSensorModuleBufferReaderFlag = tTrue;
					}  
					// get the values from Action Module
					pCoderInput->Get(SensorInputFrontBuffer, (tVoid*)&us_front);
					pCoderInput->Get(SensorInputFrontLeftBuffer, (tVoid*)&us_front_left);   
					pCoderInput->Get(SensorInputFrontRightBuffer, (tVoid*)&us_front_right);  
					pCoderInput->Get(SensorInputFrontBackBuffer, (tVoid*)&us_front_back); 
				}
				if (m_bShowLog)   
				{
					LOG_ERROR(cString::Format("Obstacle Sensor: Data: Front -> %f Front_left: %f, Front_right : %f",us_front, us_front_left,us_front_right));
				}
				if(us_front != 0)
				{
					isObjectFront = true;
				}
				else
				{
					isObjectFront = false;
				}
				if(us_front_left != 0)
				{
					isObjectLeft = true;
				}
				else
				{
					isObjectLeft = false;
				}
				if(us_front_right != 0)
				{
					isObjectRight = true;
				}
				else 
				{
					isObjectRight = false;
				}
				if(us_front_back)
				{
					isObjectBack = true;
				}
				else 
				{
					isObjectBack = false;
				}
			}
			
		}
		
    }

        RETURN_NOERROR;
}

/********************* Custom Methods *****************************/
tVoid cActionDistanceFilter::CustomCycle()
{
	if(firstTime)
	{
		firstTime = false;
		OverallDistance = 0;
	}
	else 
	{ 
		OverallDistance = OverallDistance + WheelEncoderDistance;
	}

	RunAction();

}

tResult cActionDistanceFilter::StopTheCar()
{
	TransmitAcceleration(0.0f,m_tsArduinoTime);
   // TransmitHazardIndicator(tTrue,m_tsArduinoTime);
	if(m_bShowLog)
	{
		LOG_ERROR(cString::Format("StopTheCar Called"));
	}

	RETURN_NOERROR;
}

tResult cActionDistanceFilter::ProcessInput(IMediaSample* pSample, tTimeStamp tsInputTime)
{
	if(m_ui8InitCtrl < 150)         // This loop is necessary to boot the car's controller (needs neutral signal for a certain time)
    {
		m_ui8InitCtrl++;
    }
	else
	{
		RETURN_IF_POINTER_NULL(pSample);
		const tVoid* l_pSrcBuffer;
		IplImage* oImg = cvCreateImageHeader(cvSize(m_sInputFormat.nWidth, m_sInputFormat.nHeight), IPL_DEPTH_8U, 3);
		RETURN_IF_FAILED(pSample->Lock(&l_pSrcBuffer));
		oImg->imageData = (char*)l_pSrcBuffer;
		Mat image(cvarrToMat(oImg));
		cvReleaseImage(&oImg);
		pSample->Unlock(l_pSrcBuffer);
		inputImage = image.clone();
	}
    RETURN_NOERROR;            
}

tResult cActionDistanceFilter::RunAction()
{
	if (!active_) RETURN_NOERROR;
    
	switch(DriverNextAction)
	{
		case Driver_straight:
			if(isObjectFront)
			{
				StopTheCar();
			}
			RunStraightScript();
			break;

		case Driver_left:
			if(isObjectFront)
			{
				StopTheCar();
			}
			RunTurnLeftScript();
			break;
		case Driver_right:
			if(isObjectFront || isObjectRight)
			{
				StopTheCar();
			}
			RunTurnRightScript();
			break;
		case Driver_parallel_parking:
			RunParallelParkingScript();
			break;
		case Driver_cross_parking:
			RunCrossParkingScript();
			break;
		case Driver_pull_out_left:
			if(isObjectFront)
			{
				StopTheCar();
			}
			if(m_bShowLog)
			{
				LOG_ERROR(cString::Format("Pull out left : started"));
			}
			RunPullOverLeftScript();
			break;
		case Driver_pull_out_right:
			CheckParkingTypeAndPullOutRight();
			break;
		case Driver_change_lane_left:
			if(m_bShowLog)
			{
				LOG_ERROR(cString::Format("Driver_change_lane_left : started"));
			}
			 if(isObjectLeft)
			{
				StopTheCar();
			}
			RunChangeLaneLeftScript();
			break;
		case Driver_change_lane_right:
			if(isObjectRight)
			{
				StopTheCar();
			}
			RunChangeLaneRightScript();
			break;
	}
   

  RETURN_NOERROR;
}

tResult cActionDistanceFilter::CreateInputPins(__exception)
{
	cObjectPtr<IMediaDescriptionManager> pDescManager;
	RETURN_IF_FAILED(_runtime->GetObject(OID_ADTF_MEDIA_DESCRIPTION_MANAGER,IID_ADTF_MEDIA_DESCRIPTION_MANAGER,(tVoid**)&pDescManager,__exception_ptr));

	 // Video Input
    RETURN_IF_FAILED(m_iVideoInputPin.Create("Video_Input", IPin::PD_Input, static_cast<IPinEventSink*>(this)));
    RETURN_IF_FAILED(RegisterPin(&m_iVideoInputPin));

	 // Reading Action to perfrom
    tChar const * strDescIdValue = pDescManager->GetMediaDescription("tIdValue");    
    RETURN_IF_POINTER_NULL(strDescIdValue);
    cObjectPtr<IMediaType> pTypeIdValue = new cMediaType(0, 0, 0, "tIdValue", strDescIdValue,IMediaDescription::MDF_DDL_DEFAULT_VERSION);    
    
    RETURN_IF_FAILED(pTypeIdValue->GetInterface(IID_ADTF_MEDIA_TYPE_DESCRIPTION, (tVoid**)&m_pDescriptionDecisionInput)); 
    RETURN_IF_FAILED(m_iDriverAction.Create("Action",pTypeIdValue, static_cast<IPinEventSink*> (this)));
    RETURN_IF_FAILED(RegisterPin(&m_iDriverAction));


	// Signal Value 
	tChar const * strDescSignalValue = pDescManager->GetMediaDescription("tSignalValue");
    RETURN_IF_POINTER_NULL(strDescSignalValue);        
    cObjectPtr<IMediaType> pTypeSignalValue = new cMediaType(0, 0, 0, "tSignalValue", strDescSignalValue,IMediaDescription::MDF_DDL_DEFAULT_VERSION);

	// Obstacle Detection Initialzation
	tChar const * output_stream_type2 = pDescManager->GetMediaDescription("tObstacleDistanceStruct");
    RETURN_IF_POINTER_NULL(output_stream_type2);
    cObjectPtr<IMediaType> output_type_signal_value2 = new cMediaType(0, 0, 0, "tObstacleDistanceStruct",  output_stream_type2, IMediaDescription::MDF_DDL_DEFAULT_VERSION);

	 //create pin for last distance travel
    RETURN_IF_FAILED(pTypeSignalValue->GetInterface(IID_ADTF_MEDIA_TYPE_DESCRIPTION, (tVoid**)&m_pDescriptionOutputSampleDistance));
    RETURN_IF_FAILED(m_iWheelEncoderDistance.Create("distance_lastSample", pTypeSignalValue, static_cast<IPinEventSink*> (this)));	
    RETURN_IF_FAILED(RegisterPin(&m_iWheelEncoderDistance)); 

    //create pin for Overall Distance
	

     //create pin for Ultrasonic Sensors
    RETURN_IF_FAILED(output_type_signal_value2->GetInterface(IID_ADTF_MEDIA_TYPE_DESCRIPTION, (tVoid**) &m_pDescriptionOutputSensor));
	RETURN_IF_FAILED(SensorInputPin.Create("Emergency_Sensor", output_type_signal_value2, static_cast<IPinEventSink*> (this)));
    RETURN_IF_FAILED(RegisterPin(&SensorInputPin));
	

	RETURN_NOERROR;
}

tResult cActionDistanceFilter::CreateOutputPins(__exception)
{

	cObjectPtr<IMediaDescriptionManager> pDescManager;
    RETURN_IF_FAILED(_runtime->GetObject(OID_ADTF_MEDIA_DESCRIPTION_MANAGER,IID_ADTF_MEDIA_DESCRIPTION_MANAGER,(tVoid**)&pDescManager,__exception_ptr));

    // Media Description Signal Value
    tChar const * strDescSignalValue = pDescManager->GetMediaDescription("tSignalValue");
    RETURN_IF_POINTER_NULL(strDescSignalValue);        
    cObjectPtr<IMediaType> pTypeSignalValue = new cMediaType(0, 0, 0, "tSignalValue", strDescSignalValue,IMediaDescription::MDF_DDL_DEFAULT_VERSION);    
    RETURN_IF_FAILED(pTypeSignalValue->GetInterface(IID_ADTF_MEDIA_TYPE_DESCRIPTION, (tVoid**)&m_pCoderDescSignal)); 
	
	// Media Description Signal Bool
	tChar const * strDescSignalBoolValue = pDescManager->GetMediaDescription("tBoolSignalValue");
    RETURN_IF_POINTER_NULL(strDescSignalBoolValue);
    cObjectPtr<IMediaType> pTypeSignalBoolValue = new cMediaType(0, 0, 0, "tBoolSignalValue", strDescSignalBoolValue,IMediaDescription::MDF_DDL_DEFAULT_VERSION);    
    RETURN_IF_FAILED(pTypeSignalBoolValue->GetInterface(IID_ADTF_MEDIA_TYPE_DESCRIPTION, (tVoid**)&m_pCoderDescBool));


	   //Acceleration Output
    RETURN_IF_FAILED(m_oAccelerateOutputPin.Create("Acceleration", pTypeSignalValue, static_cast<IPinEventSink*> (this)));
    RETURN_IF_FAILED(RegisterPin(&m_oAccelerateOutputPin));
    
    // Steer Angle Output pin
    RETURN_IF_FAILED(m_oSteerOutputPin.Create("Steering_Angle", pTypeSignalValue, static_cast<IPinEventSink*> (this)));
    RETURN_IF_FAILED(RegisterPin(&m_oSteerOutputPin));

	 // Left Indicator Output pin
    RETURN_IF_FAILED(m_oTurnSignalLeftOutputPin.Create("Left_Indicator", pTypeSignalBoolValue, static_cast<IPinEventSink*> (this)));
    RETURN_IF_FAILED(RegisterPin(&m_oTurnSignalLeftOutputPin));

	// Right Indicator Output Pin
    RETURN_IF_FAILED(m_oTurnSignalRightOutputPin.Create("Right_Indicator", pTypeSignalBoolValue, static_cast<IPinEventSink*> (this)));
    RETURN_IF_FAILED(RegisterPin(&m_oTurnSignalRightOutputPin));

	 RETURN_IF_FAILED(m_oReverseIndicatorOutputPin.Create("Reverse_Indicator", pTypeSignalBoolValue, static_cast<IPinEventSink*> (this)));
    RETURN_IF_FAILED(RegisterPin(&m_oReverseIndicatorOutputPin));

	 RETURN_IF_FAILED(HazardIndicatorPin.Create("Hazard_Indicator", pTypeSignalBoolValue, static_cast<IPinEventSink*> (this)));
    RETURN_IF_FAILED(RegisterPin(&HazardIndicatorPin));

	// Action Performed Output Pin
    RETURN_IF_FAILED(m_oActionPerformed.Create("Action_Performed", pTypeSignalBoolValue, static_cast<IPinEventSink*> (this)));
    RETURN_IF_FAILED(RegisterPin(&m_oActionPerformed));

    RETURN_IF_FAILED(StartLaneTrackingPin.Create("Start_LaneTracking", pTypeSignalBoolValue, static_cast<IPinEventSink*> (this)));
    RETURN_IF_FAILED(RegisterPin(&StartLaneTrackingPin));

	RETURN_NOERROR;
}

tResult cActionDistanceFilter::ReadProperties()
{

	/********************* Properties for Show Log **************************/
	m_bShowLog = GetPropertyBool(AF_PROP_SHOW_LOG);
	lastCommand = GetPropertyInt(AF_PROP_LAST_COMMAND);
	if(lastCommand != -1)
	{
		prevDriverAction = lastCommand;
        
	}
    if(m_bShowLog)
		{
			LOG_ERROR(cString::Format("prevDriverAction %d", prevDriverAction));
		}
    RETURN_NOERROR;
}

void cActionDistanceFilter::GenerateIPM(Mat inputImg, Mat &outputImg)
{
	vector<Point2f> origPoints;
	origPoints.push_back(Point2f(-100, HEIGHT));
	origPoints.push_back(Point2f(WIDTH + 100, HEIGHT));
	origPoints.push_back(Point2f(WIDTH / 2 + 180, 280));
	origPoints.push_back(Point2f(WIDTH / 2 - 220, 280));

	 //The 4-points correspondences in the destination image
	vector<Point2f> dstPoints;
	dstPoints.push_back(Point2f(0, HEIGHT));
	dstPoints.push_back(Point2f(WIDTH, HEIGHT));
	dstPoints.push_back(Point2f(WIDTH , 0));
	dstPoints.push_back(Point2f(0, 0));

	//IPM object
	IPM ipm(Size(WIDTH, HEIGHT), Size(WIDTH , HEIGHT), origPoints, dstPoints);

	Mat inputImgGray;
	// Color Conversion
	if (inputImg.channels() == 3)
		cvtColor(inputImg, inputImgGray, CV_BGR2GRAY);
	else
		inputImg.copyTo(inputImgGray);

	ipm.applyHomography(inputImg, outputImg);
	ipm.drawPoints(origPoints, inputImg);
}

int cActionDistanceFilter::CrossedParking()
{
	int result = NOT_FOUND;
	//float threshValue = 100.0;
	Mat gray, binary;
	Mat outputImg;

if(m_bShowLog)
{
    String imgPath = "/home/aadc/Desktop/images/" + patch::to_string(imageCount++) + ".png" ;
	imwrite(imgPath, inputImage);
}
	
	
	GenerateIPM(inputImage.clone(), outputImg);
		
	//Mat image = im(Rect(0, _horizon, width, _roi_height));
	cvtColor(outputImg, gray, CV_BGR2GRAY);
	GaussianBlur(gray, gray, Size(3, 3), 0, 0);
	gray = DAEDALUS::correctGamma(gray, 0.4);

	ParkingType parkingType;
	result = parkingType.IsCrossedParking(gray);
	
	return result;
}


tResult cActionDistanceFilter::TransmitAcceleration(tFloat32 f32Acceleration, tTimeStamp tsInputTime)
{
    __synchronized_obj(m_oTransmitAccelCritSection);

    //create new media sample
    cObjectPtr<IMediaSample> pSampleAccel;
    RETURN_IF_FAILED(AllocMediaSample((tVoid**)&pSampleAccel));

    //allocate memory with the size given by the descriptor
    cObjectPtr<IMediaSerializer> pSerializer;
    m_pCoderDescSignal->GetMediaSampleSerializer(&pSerializer);
    tInt nSize = pSerializer->GetDeserializedSize();
    RETURN_IF_FAILED(pSampleAccel->AllocBuffer(nSize));
        
    {
          __adtf_sample_write_lock_mediadescription(m_pCoderDescSignal, pSampleAccel, pCoderOutput);
 
          pCoderOutput->Set("f32Value", (tVoid*)&f32Acceleration);    
          pCoderOutput->Set("ui32ArduinoTimestamp", (tVoid*)&m_tsArduinoTime);    
    }
          
     pSampleAccel->SetTime(tsInputTime);
     RETURN_IF_FAILED(m_oAccelerateOutputPin.Transmit(pSampleAccel));

     RETURN_NOERROR;
}

tResult cActionDistanceFilter::TransmitSteeringAngle(const tFloat32 f32SteeringAngle, tTimeStamp tsInputTime)
{
    __synchronized_obj(m_oTransmitSteerCritSection);
    
    //create new media sample
    cObjectPtr<IMediaSample> pNewMediaSample;
    RETURN_IF_FAILED(AllocMediaSample((tVoid**)&pNewMediaSample));

    //allocate memory with the size given by the descriptor
    cObjectPtr<IMediaSerializer> pSerializer;
    m_pCoderDescSignal->GetMediaSampleSerializer(&pSerializer);
    tInt nSize = pSerializer->GetDeserializedSize();
    RETURN_IF_FAILED(pNewMediaSample->AllocBuffer(nSize));

    {
          __adtf_sample_write_lock_mediadescription(m_pCoderDescSignal, pNewMediaSample, pCoderOutput);    
 
          pCoderOutput->Set("f32Value", (tVoid*)&f32SteeringAngle);    
          pCoderOutput->Set("ui32ArduinoTimestamp", (tVoid*)&m_tsArduinoTime);    
    }
    
    pNewMediaSample->SetTime(tsInputTime);
    RETURN_IF_FAILED(m_oSteerOutputPin.Transmit(pNewMediaSample));

    //LOG_INFO(cString::Format("Sending SteeringAngle: %f", SteeringAngle).GetPtr());

    RETURN_NOERROR;
    
}

tResult cActionDistanceFilter::TransmitStartLaneTracking(const tBool value, tTimeStamp tsInputTime)
{
    
    __synchronized_obj(m_oTransmitStartLaneCritSection);
    
    //create new media sample
    cObjectPtr<IMediaSample> pNewMediaSample;
    RETURN_IF_FAILED(AllocMediaSample((tVoid**)&pNewMediaSample));

    //allocate memory with the size given by the descriptor
    cObjectPtr<IMediaSerializer> pSerializer;
    m_pCoderDescBool->GetMediaSampleSerializer(&pSerializer);
    tInt nSize = pSerializer->GetDeserializedSize();
    RETURN_IF_FAILED(pNewMediaSample->AllocBuffer(nSize));

    {
          __adtf_sample_write_lock_mediadescription(m_pCoderDescBool, pNewMediaSample, pCoderOutput);    
 
          pCoderOutput->Set("bValue", (tVoid*)&value);    
          pCoderOutput->Set("ui32ArduinoTimestamp", (tVoid*)&m_tsArduinoTime);    
    }
              
    pNewMediaSample->SetTime(_clock->GetStreamTime());
    RETURN_IF_FAILED(StartLaneTrackingPin.Transmit(pNewMediaSample));
    RETURN_NOERROR;    
}

tResult cActionDistanceFilter::TransmitRightIndicator(const tBool bTurnRightIndicator, tTimeStamp tsInputTime)
{
    __synchronized_obj(m_oTransmitRightIndicatorCritSection);
    
    //create new media sample
    cObjectPtr<IMediaSample> pNewMediaSample;
    RETURN_IF_FAILED(AllocMediaSample((tVoid**)&pNewMediaSample));

    //allocate memory with the size given by the descriptor
    cObjectPtr<IMediaSerializer> pSerializer;
    m_pCoderDescBool->GetMediaSampleSerializer(&pSerializer);
    tInt nSize = pSerializer->GetDeserializedSize();
    RETURN_IF_FAILED(pNewMediaSample->AllocBuffer(nSize));

    {
          __adtf_sample_write_lock_mediadescription(m_pCoderDescBool, pNewMediaSample, pCoderOutput);    
 
          pCoderOutput->Set("bValue", (tVoid*)&bTurnRightIndicator);    
          pCoderOutput->Set("ui32ArduinoTimestamp", (tVoid*)&m_tsArduinoTime);    
    }
              
    pNewMediaSample->SetTime(tsInputTime);
    RETURN_IF_FAILED(m_oTurnSignalRightOutputPin.Transmit(pNewMediaSample));

    RETURN_NOERROR;
    
}

tResult cActionDistanceFilter::TransmitLeftIndicator(const tBool bTurnLeftIndicator, tTimeStamp tsInputTime)
{
    __synchronized_obj(m_oTransmitLeftIndicatorCritSection);
    
    //create new media sample
    cObjectPtr<IMediaSample> pNewMediaSample;
    RETURN_IF_FAILED(AllocMediaSample((tVoid**)&pNewMediaSample));

    //allocate memory with the size given by the descriptor
    cObjectPtr<IMediaSerializer> pSerializer;
    m_pCoderDescBool->GetMediaSampleSerializer(&pSerializer);
    tInt nSize = pSerializer->GetDeserializedSize();
    RETURN_IF_FAILED(pNewMediaSample->AllocBuffer(nSize));

    {
          __adtf_sample_write_lock_mediadescription(m_pCoderDescBool, pNewMediaSample, pCoderOutput);    
 
          pCoderOutput->Set("bValue", (tVoid*)&bTurnLeftIndicator);    
          pCoderOutput->Set("ui32ArduinoTimestamp", (tVoid*)&m_tsArduinoTime);    
    }
              
    pNewMediaSample->SetTime(tsInputTime);
    RETURN_IF_FAILED(m_oTurnSignalLeftOutputPin.Transmit(pNewMediaSample));

    RETURN_NOERROR;
    
}

tResult cActionDistanceFilter::TransmitHazardIndicator(const tBool value, tTimeStamp tsInputTime)
{
    __synchronized_obj(m_oTransmitHazardIndicatorCritSection);
    
    //create new media sample
    cObjectPtr<IMediaSample> pNewMediaSample;
    RETURN_IF_FAILED(AllocMediaSample((tVoid**)&pNewMediaSample));

    //allocate memory with the size given by the descriptor
    cObjectPtr<IMediaSerializer> pSerializer;
    m_pCoderDescBool->GetMediaSampleSerializer(&pSerializer);
    tInt nSize = pSerializer->GetDeserializedSize();
    RETURN_IF_FAILED(pNewMediaSample->AllocBuffer(nSize));

    {
          __adtf_sample_write_lock_mediadescription(m_pCoderDescBool, pNewMediaSample, pCoderOutput);    
 
          pCoderOutput->Set("bValue", (tVoid*)&value);    
          pCoderOutput->Set("ui32ArduinoTimestamp", (tVoid*)&m_tsArduinoTime);    
    }
              
    pNewMediaSample->SetTime(tsInputTime);
    RETURN_IF_FAILED(HazardIndicatorPin.Transmit(pNewMediaSample));

    RETURN_NOERROR;
    
}

tResult cActionDistanceFilter::TransmitActionPerformed()
{
	tBool bAction = tTrue;
	__synchronized_obj(m_oTransmitActionPerformedCritSection);
    
    //create new media sample
    cObjectPtr<IMediaSample> pNewMediaSample;
    RETURN_IF_FAILED(AllocMediaSample((tVoid**)&pNewMediaSample));

    //allocate memory with the size given by the descriptor
    cObjectPtr<IMediaSerializer> pSerializer;
    m_pCoderDescBool->GetMediaSampleSerializer(&pSerializer);
    tInt nSize = pSerializer->GetDeserializedSize();
    RETURN_IF_FAILED(pNewMediaSample->AllocBuffer(nSize));

    {
          __adtf_sample_write_lock_mediadescription(m_pCoderDescBool, pNewMediaSample, pCoderOutput);    
 
          pCoderOutput->Set("bValue", (tVoid*)&bAction);    
          pCoderOutput->Set("ui32ArduinoTimestamp", (tVoid*)&m_tsArduinoTime);    
    }
              
    pNewMediaSample->SetTime(m_tsArduinoTime);
    RETURN_IF_FAILED(m_oActionPerformed.Transmit(pNewMediaSample));

    RETURN_NOERROR;
}

tResult cActionDistanceFilter::TransmitReverseIndicator(const tBool bTurnOnIndicator, tTimeStamp tsInputTime)
{
    __synchronized_obj(m_oTrasmitReverseIndicatorCritSection);
    
    //create new media sample
    cObjectPtr<IMediaSample> pNewMediaSample;
    RETURN_IF_FAILED(AllocMediaSample((tVoid**)&pNewMediaSample));

    //allocate memory with the size given by the descriptor
    cObjectPtr<IMediaSerializer> pSerializer;
    m_pCoderDescBool->GetMediaSampleSerializer(&pSerializer);
    tInt nSize = pSerializer->GetDeserializedSize();
    RETURN_IF_FAILED(pNewMediaSample->AllocBuffer(nSize));

    {
          __adtf_sample_write_lock_mediadescription(m_pCoderDescBool, pNewMediaSample, pCoderOutput);    
 
          pCoderOutput->Set("bValue", (tVoid*)&bTurnOnIndicator);    
          pCoderOutput->Set("ui32ArduinoTimestamp", (tVoid*)&m_tsArduinoTime);    
    }
              
    pNewMediaSample->SetTime(tsInputTime);
    RETURN_IF_FAILED(m_oReverseIndicatorOutputPin.Transmit(pNewMediaSample));

    RETURN_NOERROR;
    
}


/********************* Scripts Methods *****************************/
void cActionDistanceFilter::RunChangeLaneLeftScript()
{
    /************ GetChangeLaneProperties Properties *******************/
	GetChangeLaneProperties();
	if(m_bShowLog)
	{
		LOG_ERROR(cString::Format("RunChangeLaneLeftScript: Started"));
	}
	
	TransmitLeftIndicator(tTrue,m_tsArduinoTime);
	TransmitReverseIndicator(tTrue,m_tsArduinoTime);
	if (OverallDistance <  cl_backDuration)
	{
		if(m_bShowLog)
		{
			LOG_ERROR(cString::Format("RunChangeLaneLeftScript: Moving back"));
		}
		TransmitAcceleration(cl_backSpeed,m_tsArduinoTime);
		TransmitSteeringAngle(cl_backAngle,m_tsArduinoTime);
		//counter_++;
	} 
	else if (OverallDistance < ( cl_backDuration + cl_leftDuration ))
	{
		TransmitReverseIndicator(tFalse,m_tsArduinoTime);
		if(m_bShowLog)
		{
			LOG_ERROR(cString::Format("RunChangeLaneLeftScript: Moving Left"));
		}
		TransmitAcceleration(cl_leftSpeed,m_tsArduinoTime);
		TransmitSteeringAngle(cl_leftAngle,m_tsArduinoTime);
		//counter_++;
         
	} 
	else if (OverallDistance < ( cl_backDuration + cl_leftDuration + cl_forwardDuration ))
	{
       
		if(m_bShowLog)
		{
			LOG_ERROR(cString::Format("RunChangeLaneLeftScript: Moving Forward"));
		}
		TransmitAcceleration(cl_forwardSpeed,m_tsArduinoTime);
		TransmitSteeringAngle(cl_forwardAngle,m_tsArduinoTime);
		//counter_++;
	} 
else
	{
		TransmitLeftIndicator(tFalse,m_tsArduinoTime);
		TransmitAcceleration(0.0f,m_tsArduinoTime);
		TransmitSteeringAngle(90.0,m_tsArduinoTime);
		TransmitActionPerformed();
		counter_ = 0;
        prevDriverAction = DriverNextAction;
		active_ = false;
        DriverNextAction = -1;
		if(m_bShowLog)
		{
			LOG_ERROR(cString::Format("RunChangeLaneLeftScript: Ended"));
		}
         if(startLaneTracking)
        {
            TransmitStartLaneTracking(tTrue,m_tsArduinoTime);
            startLaneTracking = false;
        }
	}
}

void cActionDistanceFilter::RunChangeLaneRightScript()
{
    /************ GetChangeLaneProperties Properties *******************/
	GetChangeLaneProperties();
	if(m_bShowLog)
	{
		LOG_ERROR(cString::Format("RunChangeLaneRightScript: Started"));
	}
	//TransmitLight(m_oTurnSignalRightOutputPin,tTrue,m_tsArduinoTime);
	TransmitRightIndicator(tTrue,m_tsArduinoTime);
	if (OverallDistance < ( cl_rightDuration ))
	{
		if(m_bShowLog)
		{
			LOG_ERROR(cString::Format("RunChangeLaneRightScript: Moving Right"));
		}
		TransmitAcceleration(cl_rightSpeed,m_tsArduinoTime);
		TransmitSteeringAngle(cl_rightAngle,m_tsArduinoTime);
		//counter_++;
         
	} 
	else if (OverallDistance < ( cl_rightDuration +  cl_forwardDuration ))
	{
       
		if(m_bShowLog)
		{
			LOG_ERROR(cString::Format("RunChangeLaneRightScript: Moving Forward"));
		}
		TransmitAcceleration(cl_forwardSpeed,m_tsArduinoTime);
		TransmitSteeringAngle(cl_forwardAngle,m_tsArduinoTime);
		//counter_++;
	} 
else
	{
		//TransmitLight(m_oTurnSignalRightOutputPin,tFalse,m_tsArduinoTime);
		TransmitRightIndicator(tFalse,m_tsArduinoTime);
		TransmitAcceleration(0.0f,m_tsArduinoTime);
		TransmitSteeringAngle(90.0,m_tsArduinoTime);
		TransmitActionPerformed();
		counter_ = 0;
        prevDriverAction = DriverNextAction;
		active_ = false;
        DriverNextAction = -1;
		if(m_bShowLog)
		{
			LOG_ERROR(cString::Format("RunChangeLaneRightScript: Ended"));
		}
         if(startLaneTracking)
        {
            TransmitStartLaneTracking(tTrue,m_tsArduinoTime);
            startLaneTracking = false;
        }
	}
}

void cActionDistanceFilter::RunTurnLeftScript()
{
    /************ Turn Left Properties *******************/
	GetLeftTurnProperties();
	if(m_bShowLog)
	{
		LOG_ERROR(cString::Format("RunTurnLeftScript: Started"));
	}
	
	TransmitLeftIndicator(tTrue,m_tsArduinoTime);
	
	if (OverallDistance <  lt_straightDuration)
	{
		if(m_bShowLog)
		{
			LOG_ERROR(cString::Format("RunTurnLeftScript: Moving Straight"));
		}
		TransmitAcceleration(lt_straightSpeed,m_tsArduinoTime);
		TransmitSteeringAngle(lt_straightAngle,m_tsArduinoTime);
		//counter_++;
	} 
	else if (OverallDistance < ( lt_straightDuration + lt_leftDuration ))
	{
		if(m_bShowLog)
		{
			LOG_ERROR(cString::Format("RunTurnLeftScript: Moving Left"));
		}
		TransmitAcceleration(lt_leftSpeed,m_tsArduinoTime);
		TransmitSteeringAngle(lt_leftAngle,m_tsArduinoTime);
		//counter_++;
         
	} 
	else if (OverallDistance < ( lt_straightDuration + lt_leftDuration + lt_endDuration ))
	{
       
		if(m_bShowLog)
		{
			LOG_ERROR(cString::Format("RunTurnLeftScript: Moving Forward"));
		}
		TransmitAcceleration(lt_endSpeed,m_tsArduinoTime);
		TransmitSteeringAngle(lt_endAngle,m_tsArduinoTime);
		//counter_++;
	} 
else
	{
		TransmitLeftIndicator(tFalse,m_tsArduinoTime);
		TransmitAcceleration(0.0f,m_tsArduinoTime);
		TransmitSteeringAngle(90.0,m_tsArduinoTime);
		TransmitActionPerformed();
		counter_ = 0;
        prevDriverAction = DriverNextAction;
		active_ = false;
        DriverNextAction = -1;
		if(m_bShowLog)
		{
			LOG_ERROR(cString::Format("RunTurnLeftScript: Started"));
		}
         if(startLaneTracking)
        {
            TransmitStartLaneTracking(tTrue,m_tsArduinoTime);
            startLaneTracking = false;
        }
	}
}

void cActionDistanceFilter::RunTurnRightScript()
{
    /************ Turn Right Properties *******************/
	GetRightTurnProperties();
	if(m_bShowLog)
	{
		LOG_ERROR(cString::Format("RunTurnRightScript: Start"));
	}
	TransmitRightIndicator(tTrue,m_tsArduinoTime);

	if (OverallDistance < rt_straightDuration )
	 {
		 if(m_bShowLog)
		{
			LOG_ERROR(cString::Format("RunTurnRightScript: Moving Straight"));
		}
		 TransmitAcceleration(rt_straightSpeed,m_tsArduinoTime);
		 TransmitSteeringAngle(rt_straightAngle,m_tsArduinoTime);
		//counter_++;
	 } 
	else if (OverallDistance < (  rt_straightDuration + rt_rightDuration) )
	 {

		 if(m_bShowLog)
		{
			LOG_ERROR(cString::Format("RunTurnRightScript: Moving Right"));
		}
		 TransmitAcceleration(rt_rightSpeed,m_tsArduinoTime);
		 TransmitSteeringAngle(rt_rightAngle,m_tsArduinoTime);
		counter_++;
	 } 
	 else 
	 {
		  TransmitRightIndicator(tFalse,m_tsArduinoTime);
	      TransmitAcceleration(0.0f,m_tsArduinoTime);
		  TransmitSteeringAngle(rt_straightAngle,m_tsArduinoTime);
		  TransmitActionPerformed();
		  counter_ = 0;
		  active_ = false;
          prevDriverAction = DriverNextAction;
          DriverNextAction = -1;
		  if(m_bShowLog)
		  {
			  LOG_ERROR(cString::Format("RunTurnRightScript: Ended"));
		  }
           if(startLaneTracking)
        {
            TransmitStartLaneTracking(tTrue,m_tsArduinoTime);
            startLaneTracking = false;
        }
	  }

}

void cActionDistanceFilter::RunStraightScript()
{
    /************ Straight Properties *******************/
	GetStraightProperties();
	if(m_bShowLog)
	{
		LOG_ERROR(cString::Format("Action Fiter: In straight "));
	}
	if (OverallDistance <  st_straightDuration )
	{
        if(startLaneTracking)
        {
            TransmitStartLaneTracking(tTrue,m_tsArduinoTime);
            startLaneTracking = false;
        }
		TransmitAcceleration(st_straightSpeed,m_tsArduinoTime);
		TransmitSteeringAngle(st_straightAngle,m_tsArduinoTime);
		//counter_++;
	 } 
     else 
     {
		 TransmitAcceleration(0.0f,m_tsArduinoTime);
		 TransmitSteeringAngle(90.0,m_tsArduinoTime);
		 TransmitActionPerformed();
		 active_ = false;
         prevDriverAction = DriverNextAction;
         DriverNextAction = -1;
          if(m_bShowLog)
		  {
			  LOG_ERROR(cString::Format("RunTurnRightScript: Ended"));
		  }
           if(startLaneTracking)
        {
            TransmitStartLaneTracking(tTrue,m_tsArduinoTime);
            startLaneTracking = false;
        }
	}
}

void cActionDistanceFilter::RunCrossParkingScript()
{
    /************ Cross Parking Properties *******************/
	GetCrossParkingProperties();
	TransmitRightIndicator(tTrue,m_tsArduinoTime);
	if(m_bShowLog)
	{
		LOG_ERROR(cString::Format("RunCrossParkingScript: Cross Parking Started"));
	}
	if (OverallDistance <  cp_straightDuration )
	{
		TransmitAcceleration(cp_straightSpeed,m_tsArduinoTime);
		TransmitSteeringAngle(cp_straightAngle,m_tsArduinoTime);
		//counter_++;
	} 
	else if (OverallDistance < ( cp_straightDuration + cp_leftDuration) )
	{
		TransmitAcceleration(cp_leftSpeed,m_tsArduinoTime);
		TransmitSteeringAngle(cp_leftAngle,m_tsArduinoTime);
		//counter_++;
	}
	else if (OverallDistance <( cp_straightDuration + cp_leftDuration + cp_rightDuration) )
	{
		//TransmitHazardIndicator(tTrue,m_tsArduinoTime);
		//TransmitReverseIndicator(tTrue,m_tsArduinoTime);
		TransmitAcceleration(cp_rightSpeed,m_tsArduinoTime);
		TransmitSteeringAngle(cp_rightAngle,m_tsArduinoTime);
		//counter_++;
	}
	else if (OverallDistance < ( cp_straightDuration + cp_leftDuration + cp_rightDuration + cp_backDuration))
	{
		//TransmitHazardIndicator(tTrue,m_tsArduinoTime);
	//	TransmitReverseIndicator(tTrue,m_tsArduinoTime);
		TransmitAcceleration(cp_backSpeed,m_tsArduinoTime);
		TransmitSteeringAngle(cp_backAngle,m_tsArduinoTime);
		counter_++;
	}
	else 
	{
        for(int j = 0; j < 1000; j++)
        {
            TransmitHazardIndicator(tTrue,m_tsArduinoTime);
        }
		TransmitAcceleration(0.0f,m_tsArduinoTime);
		TransmitSteeringAngle(90.0,m_tsArduinoTime);
		TransmitHazardIndicator(tFalse,m_tsArduinoTime);
		TransmitReverseIndicator(tFalse,m_tsArduinoTime);
		TransmitRightIndicator(tFalse,m_tsArduinoTime);
		TransmitActionPerformed();
		counter_ = 0;
		active_ = false;
        prevDriverAction = DriverNextAction;
        DriverNextAction = -1;
		if(m_bShowLog)
		{
			LOG_ERROR(cString::Format("RunCrossParkingScript: Cross Parking Ended"));
		}
	}
}

void cActionDistanceFilter::RunParallelParkingScript()
{
    /************ Parallel Parking Properties *******************/
	GetParallelParkingProperties();
	TransmitRightIndicator(tTrue,m_tsArduinoTime);
	if(m_bShowLog)
	{
		LOG_ERROR(cString::Format("RunParallelParkingScript: Parallel Parking Started"));
	}
	if (OverallDistance <  pp_straightDuration )
	{
		TransmitAcceleration(pp_straightSpeed,m_tsArduinoTime);
		TransmitSteeringAngle(pp_straightAngle,m_tsArduinoTime);
		counter_++;
	} 
	else if (OverallDistance < ( pp_straightDuration + pp_forwardLeftDuration) )
	{
		TransmitAcceleration(pp_forwardLeftSpeed ,m_tsArduinoTime);
		TransmitSteeringAngle(pp_forwardLeftAngle,m_tsArduinoTime);
		counter_++;
	}
	else if (OverallDistance < ( pp_straightDuration + pp_forwardLeftDuration + pp_rightDuration) )
	{
		TransmitAcceleration(pp_rightSpeed,m_tsArduinoTime);
		TransmitSteeringAngle(pp_rightAngle,m_tsArduinoTime);
		counter_++;
	}
	else if (OverallDistance <( pp_straightDuration + pp_forwardLeftDuration+ pp_leftDuration + pp_rightDuration) )
	{
		//TransmitHazardIndicator(tTrue,m_tsArduinoTime);
		//TransmitReverseIndicator(tTrue,m_tsArduinoTime);
		TransmitAcceleration(pp_leftSpeed,m_tsArduinoTime);
		TransmitSteeringAngle(pp_leftAngle,m_tsArduinoTime);
		counter_++;
	}
	else if (OverallDistance < ( pp_straightDuration + pp_forwardLeftDuration + pp_leftDuration + pp_rightDuration + pp_backDuration))
	{
		//TransmitHazardIndicator(tTrue,m_tsArduinoTime);
		//TransmitReverseIndicator(tTrue,m_tsArduinoTime);
		TransmitAcceleration(pp_backSpeed,m_tsArduinoTime);
		TransmitSteeringAngle(pp_backAngle,m_tsArduinoTime);
		counter_++;
	}
	else if (OverallDistance < ( pp_straightDuration + pp_forwardLeftDuration  + pp_leftDuration + pp_rightDuration + pp_backDuration + pp_ForwardLastDuration))
	{
		//TransmitHazardIndicator(tTrue,m_tsArduinoTime);
        //TransmitStartLaneTracking(tTrue,m_tsArduinoTime);
		//TransmitReverseIndicator(tTrue,m_tsArduinoTime);
		TransmitAcceleration(pp_ForwardLastSpeed,m_tsArduinoTime);
		TransmitSteeringAngle(pp_ForwardLastAngle,m_tsArduinoTime);
		counter_++;
	}
	else 
	{
        for(int j = 0; j < 1000; j++)
        {
            TransmitHazardIndicator(tTrue,m_tsArduinoTime);
        }
		TransmitHazardIndicator(tFalse,m_tsArduinoTime);
		TransmitAcceleration(0.0f,m_tsArduinoTime);
		TransmitSteeringAngle(pp_straightAngle,m_tsArduinoTime);
		TransmitRightIndicator(tFalse,m_tsArduinoTime);
		TransmitReverseIndicator(tFalse,m_tsArduinoTime);
		TransmitActionPerformed();
		counter_ = 0;
        prevDriverAction = DriverNextAction;
        DriverNextAction = -1;
		active_ = false;
		if(m_bShowLog)
		{
			LOG_ERROR(cString::Format("RunParallelParkingScript: Parallel Parking Ended"));
		}
         if(startLaneTracking)
        {
            TransmitStartLaneTracking(tTrue,m_tsArduinoTime);
            startLaneTracking = false;
        }
	}
}

void cActionDistanceFilter::RunPullOverRightScript()
{
    /************ PullOver Right Properties *******************/
	GetPullOverRightProperties();
	TransmitRightIndicator(tTrue,m_tsArduinoTime);
	if(m_bShowLog)
	{
		LOG_ERROR(cString::Format("RunPullOverRightScript: Started"));
	}
	if (OverallDistance <  pr_straightDuration )
	{
        
		TransmitAcceleration(pr_straightSpeed,m_tsArduinoTime);
		TransmitSteeringAngle(pr_straightAngle,m_tsArduinoTime);
		//counter_++;
	} 
	else if (OverallDistance < ( pr_straightDuration + pr_rightDuration) )
	{
		TransmitAcceleration(pr_rightSpeed,m_tsArduinoTime);
		TransmitSteeringAngle(pr_rightAngle,m_tsArduinoTime);
         if(startLaneTracking)
        {
            TransmitStartLaneTracking(tTrue,m_tsArduinoTime);
            startLaneTracking = false;
        }
		//counter_++;
	}
	else 
	{
		TransmitAcceleration(0.0f,m_tsArduinoTime);
		TransmitSteeringAngle(90.0,m_tsArduinoTime);
		TransmitRightIndicator(tFalse,m_tsArduinoTime);
		TransmitActionPerformed();
		counter_ = 0;
		active_ = false;
		cp_counter = 0;
		pp_counter = 0;
         prevDriverAction = DriverNextAction;
        DriverNextAction = -1;
		if(m_bShowLog)
		{
			LOG_ERROR(cString::Format("RunPullOverRightScript: Ended"));
		}
         if(startLaneTracking)
        {
            TransmitStartLaneTracking(tTrue,m_tsArduinoTime);
            startLaneTracking = false;
        }
	}
	
}

void cActionDistanceFilter::RunPullOverLeftScript()
{
    /************ PullOver Left Properties *******************/
	GetPullOverLeftProperties();
	TransmitLeftIndicator(tTrue,m_tsArduinoTime);
	if(m_bShowLog)
	{
		LOG_ERROR(cString::Format("RunPullOverLeftScript: Started"));
	}
	if (OverallDistance <  pl_straightDuration )
	{
		TransmitAcceleration(pl_straightSpeed,m_tsArduinoTime);
		TransmitSteeringAngle(pl_straightAngle,m_tsArduinoTime);
		//counter_++;
	} 
	else if (OverallDistance < ( pl_straightDuration + pl_leftDuration) )
	{
		TransmitAcceleration(pl_leftSpeed,m_tsArduinoTime);
		TransmitSteeringAngle(pl_leftAngle,m_tsArduinoTime);
        
		//counter_++;
	}
	else 
	{
		TransmitAcceleration(0.0f,m_tsArduinoTime);
		TransmitSteeringAngle(90.0,m_tsArduinoTime);
		TransmitLeftIndicator(tFalse,m_tsArduinoTime);
		TransmitActionPerformed();
         
		counter_ = 0;
		active_ = false;
        prevDriverAction = DriverNextAction;
        DriverNextAction = -1;
		cp_counter = 0;
			pp_counter = 0;
		if(m_bShowLog)
		{
			LOG_ERROR(cString::Format("RunPullOverLeftScript: Ended"));
		}
         if(startLaneTracking)
        {
            TransmitStartLaneTracking(tTrue,m_tsArduinoTime);
            startLaneTracking = false;
        }
        
	}
}

void cActionDistanceFilter::RunPullOverRightFromParallelParkingScript()
{
		/************ PullOver Left Properties *******************/
	GetPullOverRightFromParallelParkingProperties();
	TransmitRightIndicator(tTrue,m_tsArduinoTime);
	if(m_bShowLog)
	{
		LOG_ERROR(cString::Format("RunPullOverRightFromParallelParkingScript: Started"));
	}
	if (OverallDistance <  pr_pp_backDuration )
	{
        // if(isObjectBack)
        // {
        //     StopTheCar();
        // }
		TransmitAcceleration(pr_pp_backSpeed,m_tsArduinoTime);
		TransmitSteeringAngle(pr_pp_backAngle,m_tsArduinoTime);
		counter_++;
	} 
	else if (OverallDistance < ( pr_pp_backDuration + pr_pp_leftDuration) )
	{
        //  if(isObjectBack)
        // {
        //     StopTheCar();
        // }
		TransmitAcceleration(pr_pp_leftSpeed,m_tsArduinoTime);
		TransmitSteeringAngle(pr_pp_leftAngle,m_tsArduinoTime);
		counter_++;
	}
	else if (OverallDistance < ( pr_pp_backDuration + pr_pp_leftDuration + pr_pp_rightDuration) )
	{
        //     if(isObjectBack)
        // {
        //     StopTheCar();
        // }
		TransmitAcceleration(pr_pp_rightSpeed,m_tsArduinoTime);
		TransmitSteeringAngle(pr_pp_rightAngle,m_tsArduinoTime);
       
		counter_++;
	}
	else 
	{
		TransmitAcceleration(0.0f,m_tsArduinoTime);
		TransmitSteeringAngle(90.0,m_tsArduinoTime);
		TransmitRightIndicator(tFalse,m_tsArduinoTime);
		TransmitActionPerformed();
		counter_ = 0;
		active_ = false;
        prevDriverAction = DriverNextAction;
        DriverNextAction = -1;
		cp_counter = 0;
		pp_counter = 0;
		if(m_bShowLog)
		{
			LOG_ERROR(cString::Format("RunPullOverRightFromParallelParkingScript: Ended"));
		}
         if(startLaneTracking)
        {
            TransmitStartLaneTracking(tTrue,m_tsArduinoTime);
            startLaneTracking = false;
        }
        
	}
}

void cActionDistanceFilter::RunPullOverLeftFromParallelParkingScript()
{
	/************ PullOver Right Properties *******************/
	GetPullOverLeftFromParallelParkingProperties();
	TransmitLeftIndicator(tTrue,m_tsArduinoTime);
	if(m_bShowLog)
	{
		LOG_ERROR(cString::Format("RunPullOverLeftFromParallelParkingScript: Started"));
	}
	if (OverallDistance <  pl_pp_backDuration )
	{
		TransmitAcceleration(pl_pp_backSpeed,m_tsArduinoTime);
		TransmitSteeringAngle(pl_pp_backAngle,m_tsArduinoTime);
		counter_++;
	} 
	else if (OverallDistance < ( pl_pp_backDuration + pl_pp_forwardDuration) )
	{
		TransmitAcceleration(pl_pp_forwardSpeed,m_tsArduinoTime);
		TransmitSteeringAngle(pl_pp_forwardAngle,m_tsArduinoTime);
		counter_++;
	}
	else if (OverallDistance < ( pl_pp_backDuration + pl_pp_leftDuration + pl_pp_forwardDuration) )
	{
        
            
		TransmitAcceleration(pl_pp_leftSpeed,m_tsArduinoTime);
		TransmitSteeringAngle(pl_pp_leftAngle,m_tsArduinoTime);
		counter_++;
	}
	else 
	{
		TransmitAcceleration(0.0f,m_tsArduinoTime);
		TransmitSteeringAngle(90.0,m_tsArduinoTime);
		TransmitLeftIndicator(tFalse,m_tsArduinoTime);
		TransmitActionPerformed();
		counter_ = 0;
		active_ = false;
        prevDriverAction = DriverNextAction;
        DriverNextAction = -1;
		cp_counter = 0;
		pp_counter = 0;
		if(m_bShowLog)
		{
			LOG_ERROR(cString::Format("RunPullOverLeftFromParallelParkingScript: Ended"));
		}
        if(startLaneTracking)
        {
            TransmitStartLaneTracking(tTrue,m_tsArduinoTime);
            startLaneTracking = false;
        }
	}
}

/****************** Properties Setter Methods **********************/
void cActionDistanceFilter::SetChangeLaneProperties()
{
	/************* Change Lane Left Properties *******************/
	SetPropertyFloat(AF_PROP_CL_BACK_SPEED,-0.8);
	SetPropertyFloat(AF_PROP_CL_LEFT_SPEED,0.8);
	SetPropertyFloat(AF_PROP_CL_RIGHT_SPEED,0.8);
	SetPropertyFloat(AF_PROP_CL_FORWARD_SPEED,0.0);

	SetPropertyFloat(AF_PROP_CL_BACK_ANGLE,90.0);
	SetPropertyFloat(AF_PROP_CL_LEFT_ANGLE,60.0);
	SetPropertyFloat(AF_PROP_CL_FORWARD_ANGLE,90.0);
	SetPropertyFloat(AF_PROP_CL_RIGHT_ANGLE,120.0);

	SetPropertyFloat(AF_PROP_CL_BACK_DURATION,0.5);
	SetPropertyFloat(AF_PROP_CL_LEFT_DURATION,0.4);
	SetPropertyFloat(AF_PROP_CL_FORWARD_DURATION,0.2);
	SetPropertyFloat(AF_PROP_CL_RIGHT_DURATION,0.4);
}

void cActionDistanceFilter::SetParallelParkingProperties()
{
	/************* Parallel Parking Properties *******************/
	SetPropertyFloat(AF_PROP_PARALLEL_PARKING_FORWARD_ANGLE,90.0);
	SetPropertyFloat(AF_PROP_PARALLEL_PARKING_RIGHT_ANGLE,120.0);
	SetPropertyFloat(AF_PROP_PARALLEL_PARKING_LEFT_ANGLE,60.0);
	SetPropertyFloat(AF_PROP_PARALLEL_PARKING_BACKWARD_ANGLE,90.0);
	SetPropertyFloat(AF_PROP_PARALLEL_PARKING_FORWARD_LEFT_ANGLE,60.0);
	SetPropertyFloat(AF_PROP_PARALLEL_PARKING_FORWARD_LAST_ANGLE,120.0);
	
	SetPropertyFloat(AF_PROP_PARALLEL_PARKING_FORWARD_LEFT_SPEED,0.6);
	SetPropertyFloat(AF_PROP_PARALLEL_PARKING_FORWARD_SPEED,1.0);
	SetPropertyFloat(AF_PROP_PARALLEL_PARKING_RIGHT_SPEED,-0.6);
	SetPropertyFloat(AF_PROP_PARALLEL_PARKING_LEFT_SPEED,-0.6);
	SetPropertyFloat(AF_PROP_PARALLEL_PARKING_BACKWARD_SPEED,-0.6);
	SetPropertyFloat(AF_PROP_PARALLEL_PARKING_FORWARD_LAST_SPEED,-0.6);

	SetPropertyFloat(AF_PROP_PARALLEL_PARKING_DURATION_FORWARD,0.5);
	SetPropertyFloat(AF_PROP_PARALLEL_PARKING_DURATION_LEFT_TURNING,0.3);
	SetPropertyFloat(AF_PROP_PARALLEL_PARKING_DURATION_RIGHT_TURNING,0.4);
	SetPropertyFloat(AF_PROP_PARALLEL_PARKING_DURATION_BACKWARD,0.2);
	SetPropertyFloat(AF_PROP_PARALLEL_PARKING_DURATION_FORWARD_LEFT,0);
	SetPropertyFloat(AF_PROP_PARALLEL_PARKING_DURATION_FORWARD_LAST,0.15);
}

void cActionDistanceFilter::SetCrossParkingProperties()
{
	/************* Cross Parking Properties *******************/
	SetPropertyFloat(AF_PROP_CROSS_PARKING_LEFT_ANGLE,60.0);
	SetPropertyFloat(AF_PROP_CROSS_PARKING_STRAIGHT_ANGLE,90.0);
	SetPropertyFloat(AF_PROP_CROSS_PARKING_RIGHT_ANGLE,120.0);
	SetPropertyFloat(AF_PROP_CROSS_PARKING_BACK_ANGLE,90.0);

	SetPropertyFloat(AF_PROP_CROSS_PARKING_LEFT_SPEED,1.0);
	SetPropertyFloat(AF_PROP_CROSS_PARKING_STRAIGHT_SPEED,1.0);
	SetPropertyFloat(AF_PROP_CROSS_PARKING_RIGHT_SPEED,-1.0);
	SetPropertyFloat(AF_PROP_CROSS_PARKING_BACKWARD_SPEED,-1.0);


	SetPropertyFloat(AF_PROP_CROSS_PARKING_DURATION_FORWARD,0.35);
	SetPropertyFloat(AF_PROP_CROSS_PARKING_DURATION_LEFT_TURNING,0.4);
	SetPropertyFloat(AF_PROP_CROSS_PARKING_DURATION_RIGHT_TURNING,0.45);
	SetPropertyFloat(AF_PROP_CROSS_PARKING_DURATION_BACKWARD,0.35);

}

void cActionDistanceFilter::SetStraightProperties()
{
	/************* Straight Properties *****************/
	SetPropertyFloat(AF_PROP_STRAIGHT_SPEED,1.3);
	SetPropertyFloat(AF_PROP_STRAIGHT_DURATION_FORWARD,0.8);
	SetPropertyFloat(AF_PROP_STRAIGHT_STRAIGHT_ANGLE,90.0);
}

void cActionDistanceFilter::SetLeftTurnProperties()
{
	/************* Turn Left Properties ****************/
	SetPropertyFloat(AF_PROP_LEFT_ANGLE,60.0);
	SetPropertyFloat(AF_PROP_LEFT_DURATION_FORWARD,0.48);
	SetPropertyFloat(AF_PROP_LEFT_DURATION_TURNING,1.2);
	SetPropertyFloat(AF_PROP_LEFT_SPEED,0.8);
	SetPropertyFloat(AF_PROP_LEFT_STRAIGHT_ANGLE,90.0);
	SetPropertyFloat(AF_PROP_LEFT_STRAIGHT_SPEED,1.0);

	SetPropertyFloat(AF_PROP_LEFT_END_ANGLE,90.0);
	SetPropertyFloat(AF_PROP_LEFT_END_DURATION,0.0);
	SetPropertyFloat(AF_PROP_LEFT_END_SPEED,1.3);

	SetPropertyBool(AF_PROP_LEFT_ANGLE NSSUBPROP_ISCHANGEABLE,tTrue);
	SetPropertyStr(AF_PROP_LEFT_ANGLE NSSUBPROP_DESCRIPTION, "Angle for left turn");
}

void cActionDistanceFilter::SetRightTurnProperties()
{
	/************* Turn Right Properties *******************/
	SetPropertyFloat(AF_PROP_RIGHT_ANGLE,120.0);
	SetPropertyFloat(AF_PROP_RIGHT_DURATION_FORWARD,0.21);
	SetPropertyFloat(AF_PROP_RIGHT_DURATION_TURNING,1.24);
	SetPropertyFloat(AF_PROP_RIGHT_SPEED,0.9);

	SetPropertyFloat(AF_PROP_RIGHT_STRAIGHT_ANGLE,90.0);
	SetPropertyFloat(AF_PROP_RIGHT_STRAIGHT_SPEED,0.8);

}

void cActionDistanceFilter::SetPullOverLeftProperties()
{
	/************* Pull Over left Properties *******************/
	SetPropertyFloat(AF_PROP_PL_LEFT_SPEED,0.7);
	SetPropertyFloat(AF_PROP_PL_LEFT_ANGLE,60.0);
	SetPropertyFloat(AF_PROP_PL_LEFT_DURATION,1.0);

	SetPropertyFloat(AF_PROP_PL_LEFT_STRAIGHT_SPEED,0.7);
	SetPropertyFloat(AF_PROP_PL_LEFT_STRAIGHT_ANGLE,90.0);
	SetPropertyFloat(AF_PROP_PL_LEFT_DURATION_STRAIGHT,0.3);
}

void cActionDistanceFilter::SetPullOverRightProperties()
{
	/************* Pull Over Right Properties *******************/
	SetPropertyFloat(AF_PROP_PR_RIGHT_SPEED,1.0);
	SetPropertyFloat(AF_PROP_PR_RIGHT_ANGLE,120.0);
	SetPropertyFloat(AF_PROP_PR_RIGHT_DURATION,1.6);

	SetPropertyFloat(AF_PROP_PR_RIGHT_STRAIGHT_SPEED,1.0);
	SetPropertyFloat(AF_PROP_PR_RIGHT_STRAIGHT_ANGLE,90.0);
	SetPropertyFloat(AF_PROP_PR_RIGHT_DURATION_STRAIGHT,0.0);
}

tVoid cActionDistanceFilter::SetPullOverRightFromParallelParkingProperties()
{
	/************* Pull Over Parallel Right Properties *******************/
	SetPropertyFloat(AF_PROP_PR_PP_BACK_SPEED,-1.0);
	SetPropertyFloat(AF_PROP_PR_PP_LEFT_SPEED,1.0);
	SetPropertyFloat(AF_PROP_PR_PP_RIGHT_SPEED,1.0);

	SetPropertyFloat(AF_PROP_PR_PP_BACK_ANGLE,90.0);
	SetPropertyFloat(AF_PROP_PR_PP_LEFT_ANGLE,120.0);
	SetPropertyFloat(AF_PROP_PR_PP_RIGHT_ANGLE,60.0);

	SetPropertyFloat(AF_PROP_PR_PP_BACK_DURATION,0.2);
	SetPropertyFloat(AF_PROP_PR_PP_LEFT_DURATION,0.6);
	SetPropertyFloat(AF_PROP_PR_PP_RIGHT_DURATION,0.4);
}

void cActionDistanceFilter::SetPullOverLeftFromParallelParkingProperties()
{
		/************* Pull Over Parallel left Properties *******************/
	SetPropertyFloat(AF_PROP_PL_PP_BACK_SPEED,-1.0);
	SetPropertyFloat(AF_PROP_PL_PP_FORWARD_SPEED,1.0);
	SetPropertyFloat(AF_PROP_PL_PP_LEFT_SPEED,1.0);

	SetPropertyFloat(AF_PROP_PL_PP_BACK_ANGLE,120.0);
	SetPropertyFloat(AF_PROP_PL_PP_FORWARD_ANGLE,60.0);
	SetPropertyFloat(AF_PROP_PL_PP_LEFT_ANGLE,60.0);

	SetPropertyFloat(AF_PROP_PL_PP_BACK_DURATION,0.05);
	SetPropertyFloat(AF_PROP_PL_PP_FORWARD_DURATION,0.2);	
	SetPropertyFloat(AF_PROP_PL_PP_LEFT_DURATION,1.2);
	
}

/****************** Properties Getter Methods **********************/
tVoid cActionDistanceFilter::GetChangeLaneProperties()
{
	 /************ Change Lane Properties ****************/
	cl_backAngle = GetPropertyFloat(AF_PROP_CL_BACK_ANGLE);
	cl_forwardAngle = GetPropertyFloat(AF_PROP_CL_FORWARD_ANGLE);
	cl_rightAngle = GetPropertyFloat(AF_PROP_CL_RIGHT_ANGLE);
	cl_leftAngle = GetPropertyFloat(AF_PROP_CL_LEFT_ANGLE);

	cl_backSpeed = GetPropertyFloat(AF_PROP_CL_BACK_SPEED);
	cl_forwardSpeed = GetPropertyFloat(AF_PROP_CL_FORWARD_SPEED);
	cl_rightSpeed = GetPropertyFloat(AF_PROP_CL_RIGHT_SPEED);
	cl_leftSpeed = GetPropertyFloat(AF_PROP_CL_LEFT_SPEED);

	cl_backDuration = GetPropertyFloat(AF_PROP_CL_BACK_DURATION);
	cl_forwardDuration = GetPropertyFloat(AF_PROP_CL_FORWARD_DURATION);
	cl_rightDuration = GetPropertyFloat(AF_PROP_CL_RIGHT_DURATION);
	cl_leftDuration = GetPropertyFloat(AF_PROP_CL_LEFT_DURATION);
}

void cActionDistanceFilter::GetStraightProperties()
{
    /************ Straight Properties ****************/
	st_straightAngle = GetPropertyFloat(AF_PROP_STRAIGHT_STRAIGHT_ANGLE);
	st_straightDuration = GetPropertyFloat(AF_PROP_STRAIGHT_DURATION_FORWARD);
	st_straightSpeed = GetPropertyFloat(AF_PROP_STRAIGHT_SPEED);
}

void cActionDistanceFilter::GetLeftTurnProperties()
{
    /************ Left Turn Properties ****************/
	lt_leftAngle = GetPropertyFloat(AF_PROP_LEFT_ANGLE);
	lt_leftDuration = GetPropertyFloat(AF_PROP_LEFT_DURATION_TURNING);
	lt_leftSpeed = GetPropertyFloat(AF_PROP_LEFT_SPEED);

	lt_straightAngle = GetPropertyFloat(AF_PROP_LEFT_STRAIGHT_ANGLE);
	lt_straightDuration = GetPropertyFloat(AF_PROP_LEFT_DURATION_FORWARD);
	lt_straightSpeed = GetPropertyFloat(AF_PROP_LEFT_STRAIGHT_SPEED);

	lt_endSpeed =  GetPropertyFloat(AF_PROP_LEFT_END_SPEED);
	lt_endDuration =  GetPropertyFloat(AF_PROP_LEFT_END_DURATION);
	lt_endAngle =  GetPropertyFloat(AF_PROP_LEFT_END_ANGLE);
	
}

void cActionDistanceFilter::GetRightTurnProperties()
{
    /************ Right Turn Properties ****************/
	rt_rightAngle = GetPropertyFloat(AF_PROP_RIGHT_ANGLE);
	rt_rightDuration = GetPropertyFloat(AF_PROP_RIGHT_DURATION_TURNING);
	rt_rightSpeed = GetPropertyFloat(AF_PROP_RIGHT_SPEED);

	rt_straightAngle = GetPropertyFloat(AF_PROP_RIGHT_STRAIGHT_ANGLE);
	rt_straightDuration = GetPropertyFloat(AF_PROP_RIGHT_DURATION_FORWARD);
	rt_straightSpeed = GetPropertyFloat(AF_PROP_RIGHT_STRAIGHT_SPEED);
}

void cActionDistanceFilter::GetCrossParkingProperties()
{
    /************ Cross Parking Properties ****************/
	cp_leftAngle =  GetPropertyFloat(AF_PROP_CROSS_PARKING_LEFT_ANGLE);
	cp_straightAngle =  GetPropertyFloat(AF_PROP_CROSS_PARKING_STRAIGHT_ANGLE);
	cp_rightAngle =  GetPropertyFloat(AF_PROP_CROSS_PARKING_RIGHT_ANGLE);
	cp_backAngle  =  GetPropertyFloat(AF_PROP_CROSS_PARKING_BACK_ANGLE);

	cp_leftSpeed = GetPropertyFloat(AF_PROP_CROSS_PARKING_LEFT_SPEED);
	cp_straightSpeed = GetPropertyFloat(AF_PROP_CROSS_PARKING_STRAIGHT_SPEED);
	cp_rightSpeed = GetPropertyFloat(AF_PROP_CROSS_PARKING_RIGHT_SPEED);
	cp_backSpeed = GetPropertyFloat(AF_PROP_CROSS_PARKING_BACKWARD_SPEED);

	cp_leftDuration = GetPropertyFloat(AF_PROP_CROSS_PARKING_DURATION_LEFT_TURNING);
	cp_straightDuration = GetPropertyFloat(AF_PROP_CROSS_PARKING_DURATION_FORWARD);
	cp_rightDuration = GetPropertyFloat(AF_PROP_CROSS_PARKING_DURATION_RIGHT_TURNING);
	cp_backDuration = GetPropertyFloat(AF_PROP_CROSS_PARKING_DURATION_BACKWARD);
}

void cActionDistanceFilter::GetParallelParkingProperties()
{
	/************ Parallel Parking Properties ****************/
	pp_leftAngle =  GetPropertyFloat(AF_PROP_PARALLEL_PARKING_LEFT_ANGLE);
	pp_straightAngle =  GetPropertyFloat(AF_PROP_PARALLEL_PARKING_FORWARD_ANGLE);
	pp_rightAngle =  GetPropertyFloat(AF_PROP_PARALLEL_PARKING_RIGHT_ANGLE);
	pp_backAngle  =  GetPropertyFloat(AF_PROP_PARALLEL_PARKING_BACKWARD_ANGLE);
	pp_ForwardLastAngle = GetPropertyFloat(AF_PROP_PARALLEL_PARKING_DURATION_FORWARD_LAST);

	pp_leftSpeed = GetPropertyFloat(AF_PROP_PARALLEL_PARKING_LEFT_SPEED);
	pp_straightSpeed = GetPropertyFloat(AF_PROP_PARALLEL_PARKING_FORWARD_SPEED);
	pp_rightSpeed = GetPropertyFloat(AF_PROP_PARALLEL_PARKING_RIGHT_SPEED);
	pp_backSpeed = GetPropertyFloat(AF_PROP_PARALLEL_PARKING_BACKWARD_SPEED);
	pp_ForwardLastSpeed = GetPropertyFloat(AF_PROP_PARALLEL_PARKING_FORWARD_LAST_SPEED);
	

	pp_leftDuration = GetPropertyFloat(AF_PROP_PARALLEL_PARKING_DURATION_LEFT_TURNING);
	pp_straightDuration = GetPropertyFloat(AF_PROP_PARALLEL_PARKING_DURATION_FORWARD);
	pp_rightDuration = GetPropertyFloat(AF_PROP_PARALLEL_PARKING_DURATION_RIGHT_TURNING);
	pp_backDuration = GetPropertyFloat(AF_PROP_PARALLEL_PARKING_DURATION_BACKWARD);
	pp_ForwardLastDuration = GetPropertyFloat(AF_PROP_PARALLEL_PARKING_DURATION_FORWARD_LAST);

	pp_forwardLeftAngle = GetPropertyFloat(AF_PROP_PARALLEL_PARKING_FORWARD_LEFT_ANGLE);
	pp_forwardLeftSpeed = GetPropertyFloat(AF_PROP_PARALLEL_PARKING_FORWARD_LEFT_SPEED);
	pp_forwardLeftDuration = GetPropertyFloat(AF_PROP_PARALLEL_PARKING_DURATION_FORWARD_LEFT);
}

void cActionDistanceFilter::GetPullOverLeftProperties()
{
	/************* Pull Over Left Properties *******************/
	pl_leftAngle = GetPropertyFloat(AF_PROP_PL_LEFT_ANGLE);
	pl_leftSpeed = GetPropertyFloat(AF_PROP_PL_LEFT_SPEED);
	pl_leftDuration = GetPropertyFloat(AF_PROP_PL_LEFT_DURATION);

	pl_straightAngle = GetPropertyFloat(AF_PROP_PL_LEFT_STRAIGHT_ANGLE);
	pl_straightSpeed = GetPropertyFloat(AF_PROP_PL_LEFT_STRAIGHT_SPEED);
	pl_straightDuration = GetPropertyFloat(AF_PROP_PL_LEFT_DURATION_STRAIGHT);
	
}

void cActionDistanceFilter::GetPullOverRightProperties()
{
	/************* Pull Over Right Properties *******************/
	pr_rightAngle = GetPropertyFloat(AF_PROP_PR_RIGHT_ANGLE);
	pr_rightSpeed = GetPropertyFloat(AF_PROP_PR_RIGHT_SPEED);
	pr_rightDuration = GetPropertyFloat(AF_PROP_PR_RIGHT_DURATION);

	pr_straightAngle = GetPropertyFloat(AF_PROP_PR_RIGHT_STRAIGHT_ANGLE);
	pr_straightSpeed = GetPropertyFloat(AF_PROP_PR_RIGHT_STRAIGHT_SPEED);
	pr_straightDuration = GetPropertyFloat(AF_PROP_PR_RIGHT_DURATION_STRAIGHT);
}

void cActionDistanceFilter::GetPullOverRightFromParallelParkingProperties()
{
	/************* Pull Over Parallel left Properties *******************/
	pr_pp_backSpeed = GetPropertyFloat(AF_PROP_PR_PP_BACK_SPEED);
	pr_pp_leftSpeed = GetPropertyFloat(AF_PROP_PR_PP_LEFT_SPEED);
	pr_pp_rightSpeed = GetPropertyFloat(AF_PROP_PR_PP_RIGHT_SPEED);

	pr_pp_backAngle = GetPropertyFloat(AF_PROP_PR_PP_BACK_ANGLE);
	pr_pp_leftAngle = GetPropertyFloat(AF_PROP_PR_PP_LEFT_ANGLE);
	pr_pp_rightAngle = GetPropertyFloat(AF_PROP_PR_PP_RIGHT_ANGLE);

	pr_pp_backDuration = GetPropertyFloat(AF_PROP_PR_PP_BACK_DURATION);
	pr_pp_leftDuration = GetPropertyFloat(AF_PROP_PR_PP_LEFT_DURATION);
	pr_pp_rightDuration = GetPropertyFloat(AF_PROP_PR_PP_RIGHT_DURATION);
}

void cActionDistanceFilter::GetPullOverLeftFromParallelParkingProperties()
{
	/************* Pull Over Right Properties *******************/
	pl_pp_backSpeed = GetPropertyFloat(AF_PROP_PL_PP_BACK_SPEED);
	pl_pp_leftSpeed = GetPropertyFloat(AF_PROP_PL_PP_LEFT_SPEED);
	pl_pp_forwardSpeed = GetPropertyFloat(AF_PROP_PL_PP_FORWARD_SPEED);

	pl_pp_backAngle = GetPropertyFloat(AF_PROP_PL_PP_BACK_ANGLE);
	pl_pp_leftAngle = GetPropertyFloat(AF_PROP_PL_PP_LEFT_ANGLE);
	pl_pp_forwardAngle = GetPropertyFloat(AF_PROP_PL_PP_FORWARD_ANGLE);

	pl_pp_backDuration = GetPropertyFloat(AF_PROP_PL_PP_BACK_DURATION);
	pl_pp_leftDuration = GetPropertyFloat(AF_PROP_PL_PP_LEFT_DURATION);
	pl_pp_forwardDuration = GetPropertyFloat(AF_PROP_PL_PP_FORWARD_DURATION);
}

void cActionDistanceFilter::CheckParkingTypeAndPullOutRight()
{
	if(prevDriverAction == Driver_cross_parking)
	{
		RunPullOverRightScript();
	}
	else if(prevDriverAction == Driver_parallel_parking)
	{
		RunPullOverRightFromParallelParkingScript();
	}
   
	else if( (pp_counter + cp_counter) == total_counter)
	{
		if(cp_counter > pp_counter)
		{
				if(isObjectFront || isObjectRight)
			{
				StopTheCar();
			}
			RunPullOverRightScript();
		}
		else
		{
			RunPullOverRightFromParallelParkingScript();
		}
	}
	else 
	{
		if (inputImage.data && CrossedParking() == CROSS_PARKING)
		{
			cp_counter++;
		}
		else if (inputImage.data && CrossedParking() == PARALLEL_PARKING)
		{
			pp_counter++;
		}
	}
}


