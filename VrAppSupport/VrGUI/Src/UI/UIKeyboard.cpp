/************************************************************************************

Filename    :   UIKeyboard.h
Content     :
Created     :	11/3/2015
Authors     :   Eric Duhon

Copyright   :   Copyright 2015 Oculus VR, LLC. All Rights reserved.

*************************************************************************************/

#include "Kernel/OVR_Array.h"
#include "Kernel/OVR_JSON.h"
#include "Kernel/OVR_StringHash.h"
#include "UIMenu.h"
#include "UITexture.h"
#include "UIButton.h"
#include "PackageFiles.h"
#include "App.h"
#include "OVR_FileSys.h"
#include "UIKeyboard.h"
#include <memory>

using namespace OVR;

namespace
{

	class UIKeyboardLocal : public UIKeyboard
	{
	public:
		UIKeyboardLocal( OVR::OvrGuiSys &GuiSys, UIMenu * menu );

		~UIKeyboardLocal();

		void SetKeyPressHandler( KeyPressEventT eventHandler, void * userData ) override;
		bool OnKeyEvent( const int keyCode, const int repeatCount, const OVR::KeyEventType eventType ) override;
		
		bool Init( OVR::OvrGuiSys &GuiSys, const char * keyboardSet, const Vector3f & positionOffset );

	private:
		typedef void( *command_t ) ();

		static const char * CommandShift;
		static const char * CommandChangeKeyboard;
		static const char * CommandBackspace;
		static const char * CommandReturn;

		struct Key
		{
			Key( OVR::OvrGuiSys &GuiSys ) : Button( GuiSys )
			{
			}

			String DisplayCharacter;
			String DisplayShiftCharacter;
			String Action;
			String ActionShift;
			String ActionArgument;
			String Texture;
			String TexturePressed;
			float Width { 0 }; // percent
			float FontScale { 1 };
			bool Toggle { false };
			UIButton Button; // UIObjects are not safe to copy after they are inited
			UIKeyboardLocal * Parent { nullptr }; // so we can speed things up a bit on button callbacks.
		};

		struct KbdRow
		{
			float Height { 10 }; // percent
			float Offset { 0 };
			Array <Key> Keys;
		};
		struct Keyboard
		{
			String Name;
			Array <KbdRow> KeyRows;
			const UIButton * ShiftButton { nullptr };

			StringHash <UITexture> TextureTable;

			UITexture * DefaultTexture { nullptr };
			UITexture * DefaultPressedTexture { nullptr };
			Vector2f Dimensions { 1.0f, 0.5f };
		};

		void OnClick( const Key * key );
		void UpdateButtonText( const bool shiftState );
		bool BuildKeyboardData( OVR::OvrGuiSys & GuiSys, const char * keyboardPath, Keyboard * keyboard );
		bool BuildKeyboardUI( Keyboard * keyboard, const Vector3f & positionOffset );
		void LoadKeyboard( OVR::OvrGuiSys & GuiSys, const char * keyboardPath, const Vector3f & positionOffset );
		void SwitchToKeyboard( const String & name );

		UIMenu * Menu { nullptr };

		//storing pointers, UI classes don't like to be copied.
		StringHash<Keyboard *> Keyboards;
		Keyboard * CurrentKeyboard { nullptr };
		KeyPressEventT EventHandler;
		void * UserData { nullptr };
		bool PhysicalShiftDown[2];
	};

	const char * UIKeyboardLocal::CommandShift = "cmd_shift";
	const char * UIKeyboardLocal::CommandChangeKeyboard = "cmd_change_keyboard";
	const char * UIKeyboardLocal::CommandBackspace = "cmd_backspace";
	const char * UIKeyboardLocal::CommandReturn = "cmd_return";

	UIKeyboardLocal::UIKeyboardLocal( OVR::OvrGuiSys & GuiSys, UIMenu * menu ) : Menu( menu )
	{
		PhysicalShiftDown[0] = PhysicalShiftDown[1] = false;
	}

	UIKeyboardLocal::~UIKeyboardLocal()
	{
		for ( auto iter = Keyboards.Begin(); iter != Keyboards.End(); ++iter )
		{
			delete ( iter->Second );
		}
	}

	bool UIKeyboardLocal::BuildKeyboardData( OVR::OvrGuiSys & GuiSys, const char * keyboardPath,
											 Keyboard * keyboard )
	{
		MemBufferT< uint8_t > buffer;
		if ( !GuiSys.GetApp()->GetFileSys().ReadFile( keyboardPath, buffer ) )
		{
			OVR_ASSERT( false );
			return false;
		}

		JSON *kbdJson = JSON::Parse( reinterpret_cast< char * >( static_cast< uint8_t * >( buffer ) ) );
		OVR_ASSERT( kbdJson );
		if ( !kbdJson )
		{
			return false;
		}

		const JsonReader kbdJsonReader( kbdJson );
		OVR_ASSERT( kbdJsonReader.IsObject() );
		if ( !kbdJsonReader.IsObject() )
		{
			return false;
		}

		keyboard->Name = kbdJsonReader.GetChildStringByName( "name", "" );
		OVR_ASSERT( !keyboard->Name.IsEmpty() ); //keyboard has to have a name
		if ( keyboard->Name.IsEmpty() )
		{
			return false;
		}

		keyboard->Dimensions.x = kbdJsonReader.GetChildFloatByName( "default_width", 1.0f );
		keyboard->Dimensions.y = kbdJsonReader.GetChildFloatByName( "default_height", 0.5f );

		const JsonReader textures( kbdJsonReader.GetChildByName( "textures" ) );
		OVR_ASSERT( textures.IsArray() );
		if ( textures.IsArray() )
		{
			//UIObjects aren't copyable, and don't implement move semantics, and in either case
			//our containers don't support move only types anyway. So take the pain
			struct TextureStringPair
			{
				String Name;
				String Path;
			};
			Array <TextureStringPair> TextureData;
			while ( !textures.IsEndOfArray() )
			{
				const JsonReader texture( textures.GetNextArrayElement() );
				OVR_ASSERT( texture.IsObject() );
				if ( texture.IsObject() )
				{
					String name = texture.GetChildStringByName( "name" );
					String texturePath = texture.GetChildStringByName( "texture" );
					OVR_ASSERT( !texturePath.IsEmpty() && !name.IsEmpty() );
					if ( !texturePath.IsEmpty() && !name.IsEmpty() )
					{
						keyboard->TextureTable.Add( name, UITexture() );
						TextureData.PushBack( TextureStringPair { name, texturePath } );
					}
				}
			}
			for ( const auto &tex : TextureData )
			{
				MemBufferT< uint8_t > texBuffer;
				if ( !GuiSys.GetApp()->GetFileSys().ReadFile( tex.Path.ToCStr(), texBuffer ) )
				{
					OVR_ASSERT( false );
					return false;
				}

				keyboard->TextureTable.GetCaseInsensitive( tex.Name )->LoadTextureFromBuffer(
					tex.Path.ToCStr(), MemBuffer( texBuffer, static_cast<int>( texBuffer.GetSize() ) ) );

			}
		}
		{
			String defTexture = kbdJsonReader.GetChildStringByName( "default_texture", "" );
			String defPresTexture = kbdJsonReader.GetChildStringByName( "default_pressed_texture", "" );
			OVR_ASSERT( !defTexture.IsEmpty() );
			if ( defTexture.IsEmpty() )
			{
				return false;
			}

			keyboard->DefaultTexture = keyboard->TextureTable.GetCaseInsensitive( defTexture );
			keyboard->DefaultPressedTexture = keyboard->TextureTable.GetCaseInsensitive(
				defPresTexture );

			OVR_ASSERT( keyboard->DefaultTexture ); //we have to have at least a default texture
			if ( !keyboard->DefaultTexture )
			{
				return false;
			}
			if ( !keyboard->DefaultPressedTexture )
			{
				keyboard->DefaultPressedTexture = keyboard->DefaultTexture;
			}
		}

		const JsonReader rows( kbdJsonReader.GetChildByName( "rows" ) );
		OVR_ASSERT( rows.IsArray() );
		if ( rows.IsArray() )
		{
			while ( !rows.IsEndOfArray() )
			{
				const JsonReader row( rows.GetNextArrayElement() );
				OVR_ASSERT( row.IsObject() );
				if ( row.IsObject() )
				{
					const float offset = row.GetChildFloatByName( "offset", 0.0f ) / 100.0f;
					const float height = row.GetChildFloatByName( "height", 25.0f ) / 100.0f;
					const JsonReader keys( row.GetChildByName( "keys" ) );
					OVR_ASSERT( keys.IsArray() );
					if ( keys.IsArray() )
					{
						keyboard->KeyRows.PushBack( KbdRow() );
						KbdRow &keyRow = keyboard->KeyRows.Back();
						keyRow.Height = height;
						keyRow.Offset = offset;
						while ( !keys.IsEndOfArray() )
						{
							const JsonReader key( keys.GetNextArrayElement() );
							keyRow.Keys.PushBack( Key( GuiSys ) );
							Key &akey = keyRow.Keys.Back();
							akey.Parent = this;

							akey.Width = key.GetChildFloatByName( "width", 10.0f ) / 100.0f;
							akey.FontScale = key.GetChildFloatByName( "font_scale", 1.0f );
							akey.Toggle = key.GetChildBoolByName( "toggle", false );

							//read order important for proper chain of defaults
							akey.DisplayCharacter = key.GetChildStringByName( "display_character",
																			  " " );
							akey.DisplayShiftCharacter = key.GetChildStringByName(
								"display_shift_character", akey.DisplayCharacter.ToCStr() );
							akey.Action = key.GetChildStringByName( "action",
																	akey.DisplayCharacter.ToCStr() );
							akey.ActionShift = key.GetChildStringByName( "action_shift",
																		 akey.DisplayShiftCharacter.ToCStr() );
							akey.ActionArgument = key.GetChildStringByName( "action_argument", "" );
							akey.Texture = key.GetChildStringByName( "texture", "" );
							akey.TexturePressed = key.GetChildStringByName( "texture_pressed", "" );
						}
					}
				}
			}
		}
		kbdJson->Release();

		return true;
	}

	bool UIKeyboardLocal::BuildKeyboardUI( Keyboard *keyboard, const Vector3f & positionOffset )
	{
		Vector3f UpperLeft = Vector3f { -keyboard->Dimensions.x / 2, keyboard->Dimensions.y / 2, 0.0f } + positionOffset;
		Vector3f xOffset = UpperLeft;
		for ( auto &keyRow : keyboard->KeyRows )
		{
			xOffset.x = UpperLeft.x + ( keyboard->Dimensions.x * keyRow.Offset );
			for ( auto &key : keyRow.Keys )
			{
				UIButton &button = key.Button;
				button.AddToMenu( Menu );
				UITexture *texture = keyboard->DefaultTexture;
				UITexture *texturePressed = keyboard->DefaultPressedTexture;
				if ( !key.Texture.IsEmpty() )
				{
					UITexture *overrideTexture = keyboard->TextureTable.GetCaseInsensitive(
						key.Texture );
					if ( overrideTexture )
					{
						texture = overrideTexture;
					}
				}
				if ( !key.TexturePressed.IsEmpty() )
				{
					UITexture *overrideTexture = keyboard->TextureTable.GetCaseInsensitive(
						key.TexturePressed );
					if ( overrideTexture )
					{
						texturePressed = overrideTexture;
					}
				}

				button.SetButtonImages( *texture, *texture, *texturePressed );
				button.SetButtonColors( Vector4f( 1, 1, 1, 1 ), Vector4f( 0.5f, 1, 1, 1 ),
										Vector4f( 1, 1, 1, 1 ) );
				button.SetLocalScale( Vector3f(
					( UIObject::TEXELS_PER_METER / texture->Width ) * key.Width *
					keyboard->Dimensions.x,
					( UIObject::TEXELS_PER_METER / texture->Height ) * keyboard->Dimensions.y *
					keyRow.Height, 1.0f ) );
				button.SetVisible( false );
				button.SetLocalPosition(
					xOffset + Vector3f( key.Width * keyboard->Dimensions.x / 2.0f, 0, 0 ) );
				button.SetText( key.DisplayCharacter.ToCStr() );
				if ( key.Toggle )
					button.SetAsToggleButton( *texturePressed, Vector4f( 0.5f, 1, 1, 1 ) );
				//Tend to move gaze fairly quickly when using onscreen keyboard, causes a lot of mistypes when keyboard action is on keyup instead of keydown because of this.
				button.SetActionType( UIButton::eButtonActionType::ClickOnDown );

				button.SetOnClick( [] ( UIButton *button, void *keyClicked )
				{
					Key * key = reinterpret_cast< Key * >( keyClicked );
					key->Parent->OnClick( key );
				}, &key );

				button.SetTouchDownSnd( "sv_deselect" );
				if ( key.Toggle )
				{
					button.SetTouchUpSnd( "" );
				}
				else
				{
					button.SetTouchUpSnd( "sv_select" );
				}

				VRMenuObject * menuObject = button.GetMenuObject();
				VRMenuFontParms fontParms = menuObject->GetFontParms();
				fontParms.Scale = key.FontScale;
				menuObject->SetFontParms( fontParms );
				
				if ( key.Action == CommandShift )
				{
					OVR_ASSERT(
						!keyboard->ShiftButton ); //We only support a single shift key on a keyboard for now
					keyboard->ShiftButton = &key.Button;
				}

				xOffset.x += key.Width * keyboard->Dimensions.x;
			}
			xOffset.y -= keyboard->Dimensions.y * keyRow.Height;
		}
		return true;
	}

	void UIKeyboardLocal::LoadKeyboard( OVR::OvrGuiSys &GuiSys, const char *keyboardPath, const Vector3f & positionOffset )
	{
		Keyboard *keyboard = new Keyboard();
		if ( !BuildKeyboardData( GuiSys, keyboardPath, keyboard ) )
		{
			delete keyboard;
			return;
		}

		if ( !BuildKeyboardUI( keyboard, positionOffset ) )
		{
			delete keyboard;
			return;
		}

		Keyboard ** existingKeyboard = Keyboards.GetCaseInsensitive( keyboard->Name );
		if ( existingKeyboard )
		{
			delete keyboard;
		}
		Keyboards.Set( keyboard->Name, keyboard );
		if ( !CurrentKeyboard )
			SwitchToKeyboard( keyboard->Name );
	}

	bool UIKeyboardLocal::Init( OVR::OvrGuiSys &GuiSys, const char * keyboardSet, const Vector3f & positionOffset )
	{
		MemBufferT< uint8_t > buffer;
		if ( !GuiSys.GetApp()->GetFileSys().ReadFile( keyboardSet, buffer ) )
		{
			OVR_ASSERT( false );
			return false;
		}

		JSON *kbdSetJson = JSON::Parse( reinterpret_cast< char * >( static_cast< uint8_t * >( buffer ) ) );
		OVR_ASSERT( kbdSetJson );
		if ( !kbdSetJson )
			return false;

		const JsonReader kbdSetJsonReader( kbdSetJson );
		OVR_ASSERT( kbdSetJsonReader.IsArray() );
		if ( !kbdSetJsonReader.IsArray() )
		{
			return false;
		}

		while ( !kbdSetJsonReader.IsEndOfArray() )
		{
			String keyboardPath = kbdSetJsonReader.GetNextArrayString();
			OVR_ASSERT( !keyboardPath.IsEmpty() );
			LoadKeyboard( GuiSys, keyboardPath.ToCStr(), positionOffset );
		}

		if ( !CurrentKeyboard ) //didn't get even one keyboard loaded
		{
			return false;
		}

		return true;
	}

	void UIKeyboardLocal::OnClick( const Key * key )
	{
		OVR_ASSERT( CurrentKeyboard );
		bool shiftState = false;
		if ( CurrentKeyboard->ShiftButton )
			shiftState = CurrentKeyboard->ShiftButton->IsPressed();

		const String * action = &key->Action;
		if ( shiftState && !key->ActionShift.IsEmpty() )
			action = &key->ActionShift;

		if ( *action == CommandShift )
		{
			UpdateButtonText( shiftState );
		}
		else if ( *action == CommandChangeKeyboard )
		{
			SwitchToKeyboard( key->ActionArgument.ToCStr() );
		}
		else if ( EventHandler )
		{
			if ( *action == CommandBackspace )
				EventHandler( KeyEventType::Backspace, String(), UserData );
			else if ( *action == CommandReturn )
				EventHandler( KeyEventType::Return, String(), UserData );
			else
				EventHandler( KeyEventType::Text, *action, UserData );
		}
	}

	void UIKeyboardLocal::UpdateButtonText( const bool shiftState )
	{
		OVR_ASSERT( CurrentKeyboard );
		if ( !CurrentKeyboard )
		{
			return;
		}

		for ( auto &keyRow : CurrentKeyboard->KeyRows )
		{
			for ( auto &key : keyRow.Keys )
			{
				if ( shiftState && !key.DisplayShiftCharacter.IsEmpty() )
					key.Button.SetText( key.DisplayShiftCharacter.ToCStr() );
				else
					key.Button.SetText( key.DisplayCharacter.ToCStr() );
			}
		}
	}

	void UIKeyboardLocal::SwitchToKeyboard( const String& name )
	{
		Keyboard ** newKeyboard = Keyboards.GetCaseInsensitive( name );
		OVR_ASSERT( newKeyboard );
		if ( !newKeyboard )
			return;

		if ( CurrentKeyboard )
		{
			for ( auto &keyRow : CurrentKeyboard->KeyRows )
			{
				for ( auto &key : keyRow.Keys )
				{
					key.Button.SetVisible( false );
				}
			}
		}
		CurrentKeyboard = *newKeyboard;
		OVR_ASSERT( CurrentKeyboard );
		for ( auto &keyRow : CurrentKeyboard->KeyRows )
		{
			for ( auto &key : keyRow.Keys )
			{
				key.Button.SetVisible( true );
			}
		}
	}
	
	void UIKeyboardLocal::SetKeyPressHandler( KeyPressEventT eventHandler, void * userData )
	{
		EventHandler = eventHandler;
		UserData = userData;
	}

	bool UIKeyboardLocal::OnKeyEvent( const int keyCode, const int repeatCount, const OVR::KeyEventType eventType )
	{
		if ( keyCode == OVR_KEY_LSHIFT )
		{
			PhysicalShiftDown[0] = eventType == KEY_EVENT_DOWN;
			return false;
		}
		if ( keyCode == OVR_KEY_RSHIFT )
		{
			PhysicalShiftDown[1] = eventType == KEY_EVENT_DOWN;
			return false;
		}

		if ( eventType == KEY_EVENT_DOWN )
		{
			if ( keyCode == OVR_KEY_BACKSPACE )
			{
				EventHandler( KeyEventType::Backspace, String(), UserData );
				return true;
			}
			else if ( keyCode == OVR_KEY_RETURN )
			{
				EventHandler( KeyEventType::Return, String(), UserData );
				return true;
			}
			char ascii = GetAsciiForKeyCode( ( ovrKeyCode )keyCode, PhysicalShiftDown[0] || PhysicalShiftDown[1] );
			if ( ascii )
			{
				String action;
				action.AppendChar( ascii );
				EventHandler( KeyEventType::Text, action, UserData );
				return true;
			}
		}

		return false;
	}
}

UIKeyboard * OVR::UIKeyboard::Create( OVR::OvrGuiSys &GuiSys, UIMenu * menu, const Vector3f & positionOffset )
{
	return Create( GuiSys, menu, "apk:///res/raw/default_keyboard_set.json", positionOffset );
}

UIKeyboard * OVR::UIKeyboard::Create( OVR::OvrGuiSys &GuiSys, UIMenu * menu, const char * keyboardSet, const Vector3f & positionOffset )
{
	UIKeyboardLocal * keyboard = new UIKeyboardLocal( GuiSys, menu );
	if ( !keyboard->Init( GuiSys, keyboardSet, positionOffset ) )
	{
		delete  keyboard;
		return nullptr;
	}
	return keyboard;
}

void OVR::UIKeyboard::Free( UIKeyboard * keyboard )
{
	delete keyboard;
}
