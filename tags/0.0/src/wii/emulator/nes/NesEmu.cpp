#include "main.h"

#include "Nes.h"
#include "wii_util.h"
#include "wii_input.h"
#include "wii_hw_buttons.h"

#include "wii_mednafen.h"
#include "wii_mednafen_main.h"

#ifdef WII_NETTRACE
#include <network.h>
#include "net_print.h"  
#endif

Nes::Nes() : 
  Emulator( "nes", "NES" ),
  m_configManager( *this ),
  m_dbManager( *this ),
  m_menuManager( *this ),
  m_gameGenie( false )
{
  // The emulator screen size
  m_emulatorScreenSize.w = 256;
  m_emulatorScreenSize.h = 256;

  // Set user screen sizes
  float hscale = 2.0;
  float wscale = 2.5;
  m_screenSize.w = m_defaultScreenSize.w = ((WII_WIDTH>>1)*wscale); 
  m_screenSize.h = m_defaultScreenSize.h = ((WII_HEIGHT>>1)*hscale);
}

ConfigManager& Nes::getConfigManager()
{
  return m_configManager;
}

DatabaseManager& Nes::getDbManager()
{
  return m_dbManager;
}

MenuManager& Nes::getMenuManager()
{
  return m_menuManager;
}

extern bool NESIsVSUni;
extern void MDFN_VSUniCoin();
extern MDFNGI *MDFNGameInfo;
extern int FDS_DiskInsert(int oride);
extern int FDS_DiskEject(void);
extern int FDS_DiskSelect(void);

static int flipdisk = 0;
static bool specialheld = false;

void Nes::updateControls( bool isRapid )
{
  WPAD_ScanPads();
  PAD_ScanPads();

  if( flipdisk )
  {
    switch( flipdisk )
    {
      case 30:
        FDS_DiskEject();
        break;
      case 20:
        FDS_DiskSelect();
        break;
      case 10:
        FDS_DiskInsert(-1);
        break;
    }
    flipdisk--;
  }

  bool special = false;
  for( int c = 0; c < 4; c++ )
  {
    // Check the state of the controllers
    u32 pressed = WPAD_ButtonsDown( c );
    u32 held = WPAD_ButtonsHeld( c );  
    u32 gcPressed = PAD_ButtonsDown( c );
    u32 gcHeld = PAD_ButtonsHeld( c );

    // Classic or Nunchuck?
    expansion_t exp;
    WPAD_Expansion( c, &exp );          

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
    s8 gcX = PAD_StickX( c );
    s8 gcY = PAD_StickY( c );
    s8 gcRX = PAD_SubStickX( c );
    s8 gcRY = PAD_SubStickY( c );

    if( c == 0 )
    {
      // Check for home
      if( ( pressed & WII_BUTTON_HOME ) ||
        ( gcPressed & GC_BUTTON_HOME ) ||
        wii_hw_button )
      {
        GameThreadRun = 0;
      }
    }

    u16 result = 0;

    //
    // Mapped buttons
    //
    
    StandardDbEntry* entry = (StandardDbEntry*)getDbManager().getEntry();

    for( int i = 0; i < NES_BUTTON_COUNT; i++ )
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
        u32 val = NesDbManager::NES_BUTTONS[ i ].button;
        if( val & NES_SPECIAL )
        {          
          special = true;
          if( !specialheld )
          {
            specialheld = true;
            if( NESIsVSUni )
            {
              MDFN_VSUniCoin();
            }
            else if( MDFNGameInfo->GameType == GMT_DISK && !flipdisk )
            {
              flipdisk = 30;
            }
          }                    
        }
        else 
        {
          if( !( val & BTN_RAPID ) || isRapid )
          {
            result |= ( val & 0xFFFF );          
          }
        }
      }        
    }    

    if( wii_digital_right( !isNunchuk, isClassic, heldLeft ) ||
        ( gcHeld & GC_BUTTON_RIGHT ) ||
        wii_analog_right( expX, gcX ) )
      result|=NES_RIGHT;

    if( wii_digital_left( !isNunchuk, isClassic, heldLeft ) || 
        ( gcHeld & GC_BUTTON_LEFT ) ||                       
        wii_analog_left( expX, gcX ) )
      result|=NES_LEFT;

    if( wii_digital_up( !isNunchuk, isClassic, heldLeft ) || 
        ( gcHeld & GC_BUTTON_UP ) ||
        wii_analog_up( expY, gcY ) )
      result|=NES_UP;

    if( wii_digital_down( !isNunchuk, isClassic, heldLeft ) ||
        ( gcHeld & GC_BUTTON_DOWN ) ||
        wii_analog_down( expY, gcY ) )
      result|=NES_DOWN;

    m_padData[c] = result;
  }

  if( !special )
  {
    specialheld = false;
  }
}

void Nes::onPostLoad()
{
  flipdisk = 0;
  specialheld = false;
}

bool Nes::updateDebugText( 
  char* output, const char* defaultOutput, int len )
{
  return false;
}

bool Nes::isRotationSupported()
{
  return false;
}

bool Nes::isGameGenieEnabled()
{
  return m_gameGenie;
}

void Nes::setGameGenieEnabled( bool enabled )
{
  m_gameGenie = enabled;
}

u8 Nes::getBpp()
{
  return NES_BPP;
}

bool Nes::isMultiRes()
{
  return false;
}