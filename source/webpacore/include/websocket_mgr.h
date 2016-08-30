/**
 * @file websocket_mgr.h
 *
 * @description This header defines functions required to manage WebSocket client connections.
 *
 * Copyright (c) 2015  Comcast
 */
 
#ifndef WEBSOCKET_MGR_H_
#define WEBSOCKET_MGR_H_

/**
 * @brief Interface to create WebSocket client connections.
 * Loads the WebPA config file, if not provided by the caller,
 *  and creates the intial connection and manages the connection wait, close mechanisms.
 */
void __createSocketConnection(void *config, void (* initKeypress)());

/**
 * @brief Interface to create WebSocket client connections.
 * Loads the WebPA config file and creates the intial connection and manages the connection wait, close mechanisms.
 */
void createSocketConnection();

/**
 * @brief Interface to terminate WebSocket client connections and clean up resources.
 */
void terminateSocketConnection();

#endif /* WEBSOCKET_MGR_H_ */

