/***********************************************************************
RemoteServer - Class to connect remote bathymetry and water level
viewers to an Augmented Reality Sandbox.
Copyright (c) 2019-2024 Oliver Kreylos

This file is part of the Augmented Reality Sandbox (SARndbox).

The Augmented Reality Sandbox is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The Augmented Reality Sandbox is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License along
with the Augmented Reality Sandbox; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
***********************************************************************/

#include "RemoteServer.h"

#include <Misc/SizedTypes.h>
#include <Misc/MessageLogger.h>
#include <Comm/Pipe.h>
#include <Math/Math.h>
#include <GL/gl.h>
#include <GL/GLMaterialTemplates.h>
#include <GL/GLModels.h>
#include <GL/GLGeometryWrappers.h>
#include <GL/GLTransformationWrappers.h>

#include "WaterTable2.h"
#include "Sandbox.h"
#include "IntraFrameCompressor.h"
#include "InterFrameCompressor.h"

/*************************************
Methods of class RemoteServer::Client:
*************************************/

RemoteServer::Client::Client(RemoteServer* sServer)
	:server(sServer),
	 clientPipe(server->listenSocket),
	 state(START)
	{
	clientPipe.ref();
	}

/*****************************
Methods of class RemoteServer:
*****************************/

void RemoteServer::quantizeGrid(GLsizei width,GLsizei height,const GLfloat* source,Pixel* dest)
	{
	Pixel* dEnd=dest+(height*width);
	const GLfloat* sPtr=source;
	for(Pixel* dPtr=dest;dPtr!=dEnd;++dPtr,++sPtr)
		{
		/* Scale the raw elevation value to the quantization range: */
		GLfloat se=*sPtr*eScale+eOffset;
		
		/* Clamp and quantize the scaled elevation: */
		if(se<=0.0f)
			*dPtr=Pixel(0);
		else if(se>=65535.0f)
			*dPtr=Pixel(65535U);
		else
			*dPtr=Pixel(se);
		}
	}

void RemoteServer::disconnectClient(Client* client,bool removeListener)
	{
	/* Find the client in the client list: */
	for(std::vector<Client*>::iterator cIt=clients.begin();cIt!=clients.end();++cIt)
		if(*cIt==client)
			{
			/* Reduce the number of streaming clients if the client was streaming: */
			if(client->state>=Client::INTRA)
				--numClients;
			
			if(removeListener)
				{
				/* Remove the client's event listener: */
				dispatcher.removeIOEventListener(client->listenerKey);
				}
			
			/* Remove the client from the list: */
			*cIt=clients.back();
			clients.pop_back();
			
			/* Disconnect the client: */
			delete client;
			
			break;
			}
	}

void RemoteServer::newConnectionCallback(Threads::EventDispatcher::IOEvent& event)
	{
	/* Get a pointer to the server object: */
	RemoteServer* thisPtr=static_cast<RemoteServer*>(event.getUserData());
	
	Client* newClient=0;
	try
		{
		/* Create a new client object: */
		newClient=new Client(thisPtr);
		
		/* Send an endianness token to the client: */
		newClient->clientPipe.write<Misc::UInt32>(0x12345678U);
		
		/* Send the water table's grid size and cell size to the client: */
		for(int i=0;i<2;++i)
			{
			newClient->clientPipe.write<Misc::UInt32>(thisPtr->gridSize[i]);
			newClient->clientPipe.write<Misc::Float32>(thisPtr->cellSize[i]);
			}
		
		/* Send the water table's elevation range: */
		for(int i=0;i<2;++i)
			newClient->clientPipe.write<Misc::Float32>(thisPtr->elevationRange[i]);
		
		/* Finish the message: */
		newClient->clientPipe.flush();
		
		/* Add an event listener for incoming messages from the client: */
		newClient->listenerKey=thisPtr->dispatcher.addIOEventListener(newClient->clientPipe.getFd(),Threads::EventDispatcher::Read,thisPtr->clientMessageCallback,newClient);
		
		/* Add the new client to the list: */
		thisPtr->clients.push_back(newClient);
		}
	catch(const std::runtime_error& err)
		{
		/* Disconnect the new client: */
		delete newClient;
		}
	}

void RemoteServer::clientMessageCallback(Threads::EventDispatcher::IOEvent& event)
	{
	/* Get a pointer to the client object: */
	Client* client=static_cast<Client*>(event.getUserData());
	RemoteServer* server=client->server;
	
	try
		{
		/* Handle incoming message based on the client's state: */
		switch(client->state)
			{
			case Client::START:
				{
				/* Read an endianness token: */
				Misc::UInt32 token=client->clientPipe.read<Misc::UInt32>();
				if(token==0x78563412U)
					client->clientPipe.setSwapOnRead(true);
				else if(token!=0x12345678U)
					throw std::runtime_error("Invalid endianness token");
				
				/* Go to the next state: */
				client->state=Client::INTRA;
				++server->numClients;
				break;
				}
			
			case Client::INTRA:
			case Client::INTER:
				{
				/* Read the message token: */
				unsigned int token=client->clientPipe.read<Misc::UInt16>();
				switch(token)
					{
					case 0: // Position update message
						Misc::Float32 pos[3];
						client->clientPipe.read(pos,3);
						client->position=Vrui::Point(pos);
						Misc::Float32 dir[3];
						client->clientPipe.read(dir,3);
						client->direction=Vrui::Vector(dir);
						break;
					
					default:
						throw std::runtime_error("Invalid client message");
					}
				break;
				}
			}
		}
	catch(const std::runtime_error& err)
		{
		/* Disconnect the client or something: */
		Misc::formattedConsoleWarning("RemoteServer: Disconnecting client due to exception %s",err.what());
		server->disconnectClient(client,false);
		
		/* Stop listening on the client's socket: */
		event.removeListener();
		}
	}

void* RemoteServer::communicationThreadMethod(void)
	{
	/* Dispatch events on the communications socket(s) until stopped by the main thread: */
	while(dispatcher.dispatchNextEvent())
		{
		/* Collect the current positions of all connected clients in streaming state: */
		std::vector<Vrui::ONTransform>& positions=clientPositions.startNewValue();
		positions.clear();
		Vrui::Point gridOffset(Math::div2(gridSize[0]*cellSize[0]),Math::div2(gridSize[1]*cellSize[1]));
		for(std::vector<Client*>::iterator cIt=clients.begin();cIt!=clients.end();++cIt)
			if((*cIt)->state>=Client::INTRA)
				{
				Vrui::Vector t=(*cIt)->position-gridOffset;
				Vrui::Rotation r=Vrui::Rotation::rotateFromTo(Vrui::Vector(0,0,-1),(*cIt)->direction);
				positions.push_back(Vrui::ONTransform(t,r));
				}
		clientPositions.postNewValue();
		
		/* Check if there is a new grid pair: */
		if(grids.lockNewValue())
			{
			/* Quantize the bathymetry and water level grids: */
			int newGrid=1-currentGrid;
			quantizeGrid(gridSize[0]-1,gridSize[1]-1,grids.getLockedValue().bathymetry,bathymetry[newGrid]);
			quantizeGrid(gridSize[0],gridSize[1],grids.getLockedValue().waterLevel,waterLevel[newGrid]);
			
			/* Send the quantized grid pair to all connected clients in streaming state: */
			std::vector<Client*> deadClients;
			for(std::vector<Client*>::iterator cIt=clients.begin();cIt!=clients.end();++cIt)
				{
				try
					{
					if((*cIt)->state==Client::INTRA)
						{
						/* Send the new grid pair to the client using intra-frame compression: */
						IntraFrameCompressor compressor((*cIt)->clientPipe);
						compressor.compressFrame(gridSize[0]-1,gridSize[1]-1,bathymetry[newGrid]);
						compressor.compressFrame(gridSize[0],gridSize[1],waterLevel[newGrid]);
						
						/* Finish the message: */
						(*cIt)->clientPipe.flush();
						
						/* Send grid pairs using inter-frame compression from now on: */
						(*cIt)->state=Client::INTER;
						}
					else if((*cIt)->state==Client::INTER)
						{
						/* Send the difference between the current and new grid pairs to the client using inter-frame compression: */
						InterFrameCompressor compressor((*cIt)->clientPipe);
						compressor.compressFrame(gridSize[0]-1,gridSize[1]-1,bathymetry[currentGrid],bathymetry[newGrid]);
						compressor.compressFrame(gridSize[0],gridSize[1],waterLevel[currentGrid],waterLevel[newGrid]);
						
						/* Finish the message: */
						(*cIt)->clientPipe.flush();
						}
					}
				catch(const std::runtime_error& err)
					{
					/* Disconnect the client: */
					Misc::formattedConsoleWarning("RemoteServer: Disconnecting client due to exception %s",err.what());
					deadClients.push_back(*cIt);
					}
				}
			
			/* Disconnect all dead clients: */
			for(std::vector<Client*>::iterator dcIt=deadClients.begin();dcIt!=deadClients.end();++dcIt)
				disconnectClient(*dcIt,true);
			
			currentGrid=newGrid;
			}
		}
	
	return 0;
	}

void RemoteServer::readBackCallback(GLfloat*,GLfloat*,void* userData)
	{
	RemoteServer* thisPtr=static_cast<RemoteServer*>(userData);
	
	/* Post the new grids to the grid triple buffer and wake up the communication thread: */
	thisPtr->grids.postNewValue();
	thisPtr->dispatcher.interrupt();
	}

RemoteServer::RemoteServer(Sandbox* sSandbox,int listenPortId,double sRequestInterval)
	:sandbox(sSandbox),
	 listenSocket(listenPortId,0),
	 numClients(0),
	 requestInterval(sRequestInterval),nextRequestTime(0.0)
	{
	/* Ignore SIGPIPE and leave handling of pipe errors to TCP sockets: */
	Comm::ignorePipeSignals();
	
	/* Retrieve the water table's grid and cell sizes: */
	for(int i=0;i<2;++i)
		{
		gridSize[i]=sandbox->waterTable->getSize()[i];
		cellSize[i]=sandbox->waterTable->getCellSize()[i];
		}
	
	/* Retrieve the water table's elevation range and add a safety margin: */
	elevationRange[0]=sandbox->waterTable->getDomain().min[2];
	elevationRange[1]=sandbox->waterTable->getDomain().max[2];
	GLfloat safety=(elevationRange[1]-elevationRange[0])*0.05f;
	elevationRange[0]-=safety;
	elevationRange[1]+=safety;
	
	/* Calculate elevation quantization factors: */
	eScale=65535.0f/(elevationRange[1]-elevationRange[0]);
	eOffset=0.5f-elevationRange[0]*eScale;
	
	/* Allocate the bathymetry and water level grids: */
	for(int i=0;i<3;++i)
		grids.getBuffer(i).init(gridSize);
	
	/* Create the grid quantization buffers: */
	for(int i=0;i<2;++i)
		bathymetry[i]=new Pixel[(gridSize[1]-1)*(gridSize[0]-1)];
	for(int i=0;i<2;++i)
		waterLevel[i]=new Pixel[gridSize[1]*gridSize[0]];
	currentGrid=1;
	
	/* Start listening for incoming connections on the listening socket: */
	dispatcher.addIOEventListener(listenSocket.getFd(),Threads::EventDispatcher::Read,newConnectionCallback,this);
	communicationThread.start(this,&RemoteServer::communicationThreadMethod);
	}

RemoteServer::~RemoteServer(void)
	{
	/* Shut down the communication thread: */
	dispatcher.stop();
	communicationThread.join();
	
	/* Disconnect all clients: */
	for(std::vector<Client*>::iterator cIt=clients.begin();cIt!=clients.end();++cIt)
		delete *cIt;
	
	/* Release allocated resources: */
	for(int i=0;i<2;++i)
		delete[] bathymetry[i];
	for(int i=0;i<2;++i)
		delete[] waterLevel[i];
	}

void RemoteServer::frame(double applicationTime)
	{
	/* Lock the most recent list of client positions: */
	clientPositions.lockNewValue();
	
	/* Check if it's time to request a new set of grids: */
	if(numClients>0&&applicationTime>=nextRequestTime)
		{
		/* Request new grids: */
		GridBuffers& gb=grids.startNewValue();
		if(sandbox->gridRequest.requestGrids(gb.bathymetry,gb.waterLevel,&RemoteServer::readBackCallback,this))
			{
			/* Push the next request time forward: */
			nextRequestTime=(Math::floor(applicationTime/requestInterval)+1.0)*requestInterval;
			}
		}
	}

void RemoteServer::glRenderAction(const PTransform& projection,const OGTransform& modelview,GLContextData& contextData) const
	{
	/* Draw icons for all connected clients: */
	const std::vector<Vrui::ONTransform>& positions=clientPositions.getLockedValue();
	if(!positions.empty())
		{
		glMaterialAmbientAndDiffuse(GLMaterialEnums::FRONT,GLColor<GLfloat,4>(1.0f,0.0f,0.0f));
		glMaterialSpecular(GLMaterialEnums::FRONT,GLColor<GLfloat,4>(1.0f,1.0f,1.0f));
		glMaterialShininess(GLMaterialEnums::FRONT,32.0f);
		
		/* Set up the rendering matrices: */
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadMatrix(projection);
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadMatrix(modelview);
		glMultMatrix(Geometry::invert(sandbox->boxTransform));
		
		for(std::vector<Vrui::ONTransform>::const_iterator pIt=positions.begin();pIt!=positions.end();++pIt)
			{
			glPushMatrix();
			
			/* Draw the client's position: */
			glMultMatrix(*pIt);
			glDrawSphereIcosahedron(1.0f,4);
			
			/* Draw the client's viewing direction: */
			glTranslate(0.0f,0.0f,-1.25f);
			glDrawCone(0.5f,2.0f,16);
			
			glPopMatrix();
			}
		
		/* Restore original rendering matrices: */
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();
		}
	}
