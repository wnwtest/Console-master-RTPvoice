/*
 * myrtp.cpp
 *
 * Created: 2018/01/03
 * Author: EDWARDS
 */ 

#pragma once

#include "stdafx.h"
#include "myrtp.h"

MyRTP::MyRTP()
: RecvVoiceCallBackFunc(NULL)
,poll_thread_isactive(false)
, rx_rtp_thread_p(NULL)
,payloaddata(NULL) 
,payloaddatalength(0)
,ssrc(0)
,set_thread_exit_flag(false)
//,thread_exited_flag(false)
, ondatalock(NULL)
{

#ifdef WIN32
	WSADATA dat;
	WSAStartup(MAKEWORD(2, 2), &dat);//init socket
#else
#endif
	//ondata_locker = CreateMutex(nullptr, FALSE, (LPCWSTR)"ondata");
	ondatalock = new CriSection();

}

MyRTP::~MyRTP()
{	
	SetThreadExitFlag();

	if (rx_rtp_thread_p != NULL)
	{
		delete rx_rtp_thread_p;
		rx_rtp_thread_p = NULL;
	}

	if (ondatalock != NULL)
	{
		delete ondatalock;
		ondatalock = NULL;
	}

#ifdef WIN32
	WSACleanup();
#else
#endif

	std::cout<<"Destory: MyRTP \n"<<std::endl;

}

void MyRTP::SetCallBackFunc(void(*callBackFunc)(ResponeRTPData))
{
	RecvVoiceCallBackFunc = callBackFunc;//回调设置
}

void MyRTP::onData(void(*func)(ResponeRTPData), ResponeRTPData data)
{
	//WaitForSingleObject(ondata_locker, INFINITE);
	ondatalock->Lock();
	try
	{
		func(data);
	}
	catch (double)
	{
		std::cout<<"func error...\n"<<std::endl;

	}
	ondatalock->Unlock();
	//ReleaseMutex(ondata_locker);

}



int MyRTP::OnPollThread(void* p)
{
	MyRTP *arg = (MyRTP*)p;
	if (arg != NULL)
	{
		arg->OnPollThreadFunc();
	}
	return 0;
}

void MyRTP::OnPollThreadFunc()
{
	int status = 0;
	uint32_t num =0;
	poll_thread_isactive = true;

	RTPTime delay(0.001);
	for(;;){
		
		BeginDataAccess();
			
		// check incoming packets
		if (GotoFirstSourceWithData())
		{
			do
			{
				RTPPacket *pack;
				RTPSourceData *srcdat;
				
				srcdat = GetCurrentSourceInfo();
				
				while ((pack = GetNextPacket()) != NULL)
				{
					ProcessRTPPacket(*srcdat,*pack);
					DeletePacket(pack);
				}
			} while (GotoNextSourceWithData());
		}
			
		EndDataAccess();
		status = Poll();
		CheckError(status);

		//RTPTime::Wait(RTPTime(0,20));//20ms
		RTPTime::Wait(delay);

		if (set_thread_exit_flag)break;
		
	}
	
	BYEDestroy(RTPTime(5, 0), 0, 0);
	//TRACE(("exit poll thread: 0x%x\r\n"), GetCurrentThreadId());
	std::cout << "exit poll thread: 0x" << hex << GetCurrentThreadId() << std::endl;

	//thread_exited_flag = true;
	//return 0;
	
}

void MyRTP::ProcessRTPPacket(const RTPSourceData &srcdat,const RTPPacket &rtppack)
{
	// You can inspect the packet and the source's info here
	//std::cout << "Got packet " << rtppack.GetExtendedSequenceNumber() << " from SSRC " << srcdat.GetSSRC() << std::endl;
	//std::cout<<"Got packet with extended sequence number:%d \n"), rtppack.GetExtendedSequenceNumber());
	payloaddatalength = rtppack.GetPayloadLength();
	ssrc = srcdat.GetSSRC();
	payloaddata = rtppack.GetPayloadData();
	//std::cout<<"from SSRC %d ,PayloadLength: %d\r\n"), ssrc, payloaddatalength);
	if (RecvVoiceCallBackFunc != NULL)
	{
		ResponeRTPData r = { payloaddatalength, ssrc, payloaddata};

		onData(RecvVoiceCallBackFunc, r);
	}

}

void MyRTP::RtpParamsInit(uint16_t portbase, uint16_t destport, uint32_t ssrc)
{
	
	int status = 0;
	uint8_t localip[]={127,0,0,1};
	// Now, we'll create a RTP session, set the destination, send some
	// packets and poll for incoming data.

	RTPUDPv4TransmissionParams transparams;
	RTPSessionParams sessparams;

	// IMPORTANT: The local timestamp unit MUST be set, otherwise
	//            RTCP Sender Report info will be calculated wrong
	// In this case, we'll be sending 10 samples each second, so we'll
	// put the timestamp unit to (1.0/10.0)
	sessparams.SetOwnTimestampUnit(1.0 / 8000.0);

	sessparams.SetAcceptOwnPackets(true);
	sessparams.SetUsePredefinedSSRC(true);//set SSRC for rtp-send	
	sessparams.SetPredefinedSSRC(ssrc);
	
	transparams.SetPortbase(portbase);
	status = Create(sessparams, &transparams);
	//fprintf(stderr,"status :%d\n", status);
	CheckError(status);
	
	RTPIPv4Address addr(localip, destport);

	status = AddDestination(addr);
	CheckError(status);

	rx_rtp_thread_p = new MyCreateThread(OnPollThread, this);
	//CWinThread* MyThread = AfxBeginThread(OnPollThread, this, THREAD_PRIORITY_NORMAL, 0, 0, NULL);
	//rx_rtp_handle = (HANDLE)_beginthreadex(NULL, 0, (unsigned int(__stdcall*)(void *))OnPollThread, this, 0, NULL);
	//if (rx_rtp_handle == NULL)
	//{
	//	std::cout<<"create thread failed"<<std::endl;
	//	system("pause");
	//}

	
}


void MyRTP::SetParamsForSender()
{
	
	SetDefaultPayloadType(96);//设置传输类型  
    SetDefaultMark(false);      //设置位  
    SetTimestampUnit(1.0/8000.0); //设置采样间隔  
    SetDefaultTimestampIncrement(160);//设置时间戳增加间隔  
	
	
}

void MyRTP::SendRTPPayloadData(void* buff, uint32_t buff_length)
{
	if (buff != NULL)
	{
		int status = this->SendPacket(buff, buff_length, 0, false, 160);
		CheckError(status);
	}
	
}

uint8_t *MyRTP::GetRTPPayloadData()
{
	
	return payloaddata;
}

uint32_t  MyRTP::GetRTPPayloadDataLength()
{
	return payloaddatalength;
	
}

uint32_t  MyRTP::GetRTPSSRC()
{
	return ssrc;
	
}


void MyRTP::CheckError(int rtperr)
{
	if (rtperr < 0)
	{
		std::cout << "MyRTPReceiver_ERROR: " << RTPGetErrorString(rtperr) << std::endl;
		//while (1);
		//exit(-1);
	}
}





