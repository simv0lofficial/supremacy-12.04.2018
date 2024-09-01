#include "includes.h"

bool Hooks::IsConnected( ) {
	Stack stack;

	static Address IsLoadoutAllowed{ pattern::find( g_csgo.m_client_dll, XOR( "84 C0 75 04 B0 01 5F" ) ) };

	if( g_menu.main.misc.unlock.get( ) && stack.ReturnAddress( ) == IsLoadoutAllowed )
		return false;

	return g_hooks.m_engine.GetOldMethod< IsConnected_t >( IVEngineClient::ISCONNECTED )( this );
}

bool Hooks::IsHLTV( ) {
	Stack stack;

	static Address SetupVelocity{ pattern::find( g_csgo.m_client_dll, XOR( "84 C0 75 38 8B 0D ? ? ? ? 8B 01 8B 80" ) ) };
	static Address ReevaluateAnimLOD{ pattern::find( g_csgo.m_client_dll, XOR( "84 C0 0F 85 ? ? ? ? A1 ? ? ? ? 8B B7" ) ) };
	static Address AccumulateLayers{ pattern::find( g_csgo.m_client_dll, XOR( "84 C0 75 0D F6 87" ) ) };

	// AccumulateLayers
	if ( stack.ReturnAddress( ) == AccumulateLayers )
		return true;

	// fix for animstate velocity.
	if ( stack.ReturnAddress( ) == SetupVelocity )
		return true;

	// fix for anim lods.
	if ( stack.ReturnAddress( ) == ReevaluateAnimLOD )
		return true;

	return g_hooks.m_engine.GetOldMethod< IsHLTV_t >( IVEngineClient::ISHLTV )( this );
}