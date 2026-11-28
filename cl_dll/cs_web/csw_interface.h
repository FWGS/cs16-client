#pragma once

#define CSWEB_VERSION "0.0.1"

void CSW_Init();
bool CSW_Create( int w, int h, const char *url );
void CSW_Destroy();
bool CSW_Active();
void CSW_Frame();
void CSW_Draw( float w, float h, float posX, float posY );
void CSW_Resize( int w, int h );
bool CSW_Key( int down, int keynum );
void CSW_MouseMove( int sx, int sy );
void CSW_MouseButton( int button, bool down );
void CSW_LoadURL( const char *url );