#include "includes.h"

Bones g_bones{};;

bool Bones::setup( Player* player, BoneArray* out, LagRecord* record ) {
	// if the record isnt setup yet.
	if( !record->m_setup ) {
		// run setupbones rebuilt.
		if( !BuildBones( player, 0x7FF00, record->m_bones, record ) )
			return false;

		// we have setup this record bones.
		record->m_setup = true;
	}

	// record is setup.
	if( out && record->m_setup )
		std::memcpy( out, record->m_bones, sizeof( BoneArray ) * 128 );

	return true;
}

bool Bones::BuildBones( Player* target, int mask, BoneArray* out, LagRecord* record ) {
	vec3_t           backup_origin;
	ang_t            backup_angles;
	float            backup_poses[ 24 ];
	C_AnimationLayer backup_layers[ 13 ];

	// get hdr.
	CStudioHdr* hdr = target->GetModelPtr( );
	if( !hdr )
		return false;

	// get ptr to bone accessor.
	CBoneAccessor* accessor = &target->m_BoneAccessor( );
	if( !accessor )
		return false;

	// store origial output matrix.
	// likely cachedbonedata.
	BoneArray* backup_matrix = accessor->m_pBones;
	if( !backup_matrix )
		return false;

	// prevent the game from calling ShouldSkipAnimationFrame.
	auto bSkipAnimationFrame = *reinterpret_cast< int* >( uintptr_t( target ) + 0x260 );
	*reinterpret_cast< int* >( uintptr_t( target ) + 0x260 ) = NULL;

	// backup original.
	backup_origin  = target->GetAbsOrigin( );
	backup_angles  = target->GetAbsAngles( );
	target->GetPoseParameters( backup_poses );
	target->GetAnimLayers( backup_layers );

	// set non interpolated data.
	target->AddEffect( EF_NOINTERP );
	target->SetAbsOrigin( record->m_pred_origin );
	target->SetAbsAngles( record->m_abs_ang );
	target->SetPoseParameters( record->m_poses );
	target->SetAnimLayers( record->m_layers );
	target->m_AnimOverlay( )[ 12 ].m_weight = 0.f;

	// force game to call AccumulateLayers - pvs fix.
	m_running = true;

	// setup our bones.
	target->InvalidateBoneCache( );
	target->SetupBones( out, 128, mask, record->m_pred_time );

	// restore original interpolated entity data.
	target->RemoveEffect( EF_NOINTERP );
	target->SetAbsOrigin( backup_origin );
	target->SetAbsAngles( backup_angles );
	target->SetPoseParameters( backup_poses );
	target->SetAnimLayers( backup_layers );

	// revert to old game behavior.
	m_running = false;

	// allow the game to call ShouldSkipAnimationFrame.
	*reinterpret_cast< int* >( uintptr_t( target ) + 0x260 ) = bSkipAnimationFrame;

	return true;
}