#include "main.h"

#include "Lynx.h"
#include "wii_util.h"
#include "wii_input.h"
#include "wii_hw_buttons.h"

#include "wii_mednafen.h"
#include "wii_mednafen_main.h"

Lynx::Lynx() : 
  Emulator( "lynx", "Lynx" ),
  m_configManager( *this ),
  m_dbManager( *this ),
  m_menuManager( *this )
{
  // The emulator screen size
  m_emulatorScreenSize.w = 160;
  m_emulatorScreenSize.h = 102;

  // Set user screen sizes
  float scale = 1.95;
  m_screenSize.w = m_defaultScreenSize.w = ((WII_WIDTH>>1)*scale); 
  m_screenSize.h = m_defaultScreenSize.h = ((WII_HEIGHT>>1)*scale);

  scale = 1.42;
  m_rotatedScreenSize.w = 
    m_defaultRotatedScreenSize.w = ((WII_WIDTH>>1)*scale); 
  m_rotatedScreenSize.h = 
    m_defaultRotatedScreenSize.h = ((WII_HEIGHT>>1)*scale);
}

ConfigManager& Lynx::getConfigManager()
{
  return m_configManager;
}

DatabaseManager& Lynx::getDbManager()
{
  return m_dbManager;
}

MenuManager& Lynx::getMenuManager()
{
  return m_menuManager;
}

void Lynx::updateControls( bool isRapid )
{
  WPAD_ScanPads();
  PAD_ScanPads();

  // Check the state of the controllers
  u32 pressed = WPAD_ButtonsDown( 0 );
  u32 held = WPAD_ButtonsHeld( 0 );  
  u32 gcPressed = PAD_ButtonsDown( 0 );
  u32 gcHeld = PAD_ButtonsHeld( 0 );

  // Classic or Nunchuck?
  expansion_t exp;
  WPAD_Expansion( 0, &exp );          

  BOOL isClassic = ( exp.type == WPAD_EXP_CLASSIC );
  BOOL isNunchuk = ( exp.type == WPAD_EXP_NUNCHUK );

  // Mask off the Wiimote d-pad depending on whether a nunchuk
  // is connected. (Wiimote d-pad is left when nunchuk is not
  // connected, right when it is).
  u32 heldLeft = ( isNunchuk ? ( held & ~0x0F00 ) : held );
  u32 heldRight = ( !isNunchuk ? ( held & ~0x0F00 ) : held );

  // Analog for Wii controls
  float expX = wii_exp_analog_val( &exp, TRUE, FALSE );
  float expY = wii_exp_analog_val( &exp, FALSE, FALSE );
  float expRX = isClassic ? wii_exp_analog_val( &exp, TRUE, TRUE ) : 0;
  float expRY = isClassic ? wii_exp_analog_val( &exp, FALSE, TRUE ) : 0;

  // Analog for Gamecube controls
  s8 gcX = PAD_StickX( 0 );
  s8 gcY = PAD_StickY( 0 );
  s8 gcRX = PAD_SubStickX( 0 );
  s8 gcRY = PAD_SubStickY( 0 );

  // Check for home
  if( ( pressed & WII_BUTTON_HOME ) ||
    ( gcPressed & GC_BUTTON_HOME ) ||
    wii_hw_button )
  {
    GameThreadRun = 0;
  }

  u16 result = 0;

  //
  // Mapped buttons
  //
  
  StandardDbEntry* entry = (StandardDbEntry*)getDbManager().getEntry();

  for( int i = 0; i < LYNX_BUTTON_COUNT; i++ )
  {
    if( ( held &
          ( ( isClassic ? 
                entry->appliedButtonMap[ 
                  WII_CONTROLLER_CLASSIC ][ i ] : 0 ) |
            ( isNunchuk ?
                entry->appliedButtonMap[
                  WII_CONTROLLER_CHUK ][ i ] :
                entry->appliedButtonMap[
                  WII_CONTROLLER_MOTE ][ i ] ) ) ) ||
        ( gcHeld &
            entry->appliedButtonMap[
              WII_CONTROLLER_CUBE ][ i ] ) )
    {
      u32 val = LynxDbManager::LYNX_BUTTONS[ i ].button;
      if( !( val & BTN_RAPID ) || isRapid )
      {
        result |= ( val & 0xFFFF );
      }
    }
  }    

  //
  // Left sticks and pads
  //
  int left, right, up, down;
  switch( getRotation() )
  {
    case MDFN_ROTATE0:
      up = LYNX_UP;
      left = LYNX_LEFT;
      down = LYNX_DOWN;
      right = LYNX_RIGHT;
      break;
    case MDFN_ROTATE270:
      up = LYNX_LEFT;
      left = LYNX_DOWN;
      down = LYNX_RIGHT;
      right = LYNX_UP;
      break;
    case MDFN_ROTATE90:
      up = LYNX_RIGHT;
      left = LYNX_UP;
      down = LYNX_LEFT;
      right = LYNX_DOWN;
  }

  if( wii_digital_right( !isNunchuk, isClassic, heldLeft ) ||
      ( gcHeld & GC_BUTTON_RIGHT ) ||
      wii_analog_right( expX, gcX ) )
    result|=right;

  if( wii_digital_left( !isNunchuk, isClassic, heldLeft ) || 
      ( gcHeld & GC_BUTTON_LEFT ) ||                       
      wii_analog_left( expX, gcX ) )
    result|=left;

  if( wii_digital_up( !isNunchuk, isClassic, heldLeft ) || 
      ( gcHeld & GC_BUTTON_UP ) ||
      wii_analog_up( expY, gcY ) )
    result|=up;

  if( wii_digital_down( !isNunchuk, isClassic, heldLeft ) ||
      ( gcHeld & GC_BUTTON_DOWN ) ||
      wii_analog_down( expY, gcY ) )
    result|=down;

  m_padData = result;
}

void Lynx::onPostLoad()
{
}

bool Lynx::updateDebugText( 
  char* output, const char* defaultOutput, int len )
{
  return false;
}

bool Lynx::isRotationSupported()
{
  return true;
}

int Lynx::getRotation()
{
  return 
    ((LynxDbEntry*)m_dbManager.getEntry())->orient;
}