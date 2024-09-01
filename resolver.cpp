#include "includes.h"

Resolver g_resolver{};;

LagRecord* Resolver::FindIdealRecord( AimPlayer* data ) {
    LagRecord *first_valid, *current;

	if( data->m_records.empty( ) )
		return nullptr;

    first_valid = nullptr;

    // iterate records.
	for( const auto &it : data->m_records ) {
		if( it->dormant( ) || it->immune( ) || !it->valid( ) )
			continue;

        // get current record.
        current = it.get( );

        // first record that was valid, store it for later.
        if( !first_valid )
            first_valid = current;

        // try to find a record with a shot, lby update, walking or no anti-aim.
		if( it->m_mode == Modes::RESOLVE_BODY || it->m_mode == Modes::RESOLVE_WALK )
            return current;
	}

	// none found above, return the first valid record if possible.
	return ( first_valid ) ? first_valid : nullptr;
}

LagRecord* Resolver::FindLastRecord( AimPlayer* data ) {
    LagRecord* current;

	if( data->m_records.empty( ) )
		return nullptr;

	// iterate records in reverse.
	for( auto it = data->m_records.crbegin( ); it != data->m_records.crend( ); ++it ) {
		current = it->get( );

		// if this record is valid.
		// we are done since we iterated in reverse.
		if( current->valid( ) && !current->immune( ) && !current->dormant( ) )
			return current;
	}

	return nullptr;
}

float Resolver::GetAwayAngle( LagRecord* record ) {
	ang_t  away;
	math::VectorAngles( g_cl.m_local->m_vecOrigin( ) - record->m_pred_origin, away );
	return away.y;
}

void Resolver::MatchShot( AimPlayer* data, LagRecord* record ) {
	// do not attempt to do this in nospread mode.
	if( g_menu.main.config.mode.get( ) == 1 )
		return;

	float shoot_time = -1.f;

	Weapon* weapon = data->m_player->GetActiveWeapon( );
	if( weapon ) {
		// with logging this time was always one tick behind.
		// so add one tick to the last shoot time.
		shoot_time = weapon->m_fLastShotTime( ) + g_csgo.m_globals->m_interval;
	}

	// this record has a shot on it.
	if( game::TIME_TO_TICKS( shoot_time ) == game::TIME_TO_TICKS( record->m_sim_time ) ) {
		if( record->m_lag <= 2 )
			record->m_shot = true;
		
		// more then 1 choke, cant hit pitch, apply prev pitch.
		else if( data->m_records.size( ) >= 2 ) {
			LagRecord* previous = data->m_records[ 1 ].get( );

			if( previous && !previous->dormant( ) )
				record->m_eye_angles.x = previous->m_eye_angles.x;
		}
	}
}

void Resolver::SetMode( LagRecord* record ) {
	// the resolver has 3 modes to chose from.
	// these modes will vary more under the hood depending on what data we have about the player
	// and what kind of hack vs. hack we are playing (mm/nospread).

	float speed = record->m_anim_velocity.length( );

	// if on ground, moving, and not fakewalking.
	if( ( record->m_flags & FL_ONGROUND ) && speed > 0.1f && !record->m_fake_walk )
		record->m_mode = Modes::RESOLVE_WALK;

	// if on ground, not moving or fakewalking.
	if( ( record->m_flags & FL_ONGROUND ) && ( speed <= 0.1f || record->m_fake_walk ) )
		record->m_mode = Modes::RESOLVE_STAND;

	// if not on ground.
	else if( !( record->m_flags & FL_ONGROUND ) )
		record->m_mode = Modes::RESOLVE_AIR;
}

void Resolver::ResolveAngles( Player* player, LagRecord* record ) {
	AimPlayer* data = &g_aimbot.m_players[ player->index( ) - 1 ];

	// mark this record if it contains a shot.
	MatchShot( data, record );

	// next up mark this record with a resolver mode that will be used.
	SetMode( record );

	// store the value for this record.
	record->m_away = GetAwayAngle( record);

	// we arrived here we can do the acutal resolve.
	if( record->m_mode == Modes::RESOLVE_WALK ) 
		ResolveWalk( data, record );

	else if( record->m_mode == Modes::RESOLVE_STAND )
		ResolveStand( data, record );

	else if( record->m_mode == Modes::RESOLVE_AIR )
		ResolveAir( data, record );

	// normalize the eye angles, doesn't really matter but its clean.
	math::NormalizeAngle( record->m_eye_angles.y );
}

void Resolver::ResolveWalk( AimPlayer* data, LagRecord* record ) {
	// apply lby to eyeangles.
	record->m_eye_angles.y = record->m_body;

	// delay body update.
	data->m_body_update = record->m_anim_time + 0.22f;

	// reset stand and body index.
	data->m_stand_index  = 0;
	data->m_stand_index2 = 0;
	data->m_body_index   = 0;
	data->m_stored_first = false;
	data->m_stored_diff  = 0.f;

	// reset prefer air angles.
	data->m_prefer_air.clear( );
	data->m_prefer_air.push_back( record->m_body );

	// reset prefer stand angles.
	data->m_prefer_stand.clear( );
	data->m_prefer_stand.push_back( record->m_body );

	// copy the last record that this player was walking
	// we need it later on because it gives us crucial data.
	data->m_walk_record.update( record );
}

void Resolver::ResolveStand2( AimPlayer* data, LagRecord* record ) {

	// get predicted away angle for the player.
	float away = GetAwayAngle( record );

	// stand2 -> no known last move.
	record->m_mode = Modes::RESOLVE_STAND2;

	switch( data->m_stand_index2 % 5 )
	{
	case 0:
		record->m_eye_angles.y = away + 180.f;
		break;

	case 1:
		record->m_eye_angles.y = record->m_body;
		break;

	case 2:
		record->m_eye_angles.y = record->m_body + 180.f;
		break;

	case 3:
		record->m_eye_angles.y = record->m_body + 90.f;
		break;

	case 4:
		record->m_eye_angles.y = record->m_body - 90.f;
		break;

	default:
		return;
	}
}

void Resolver::ResolveStand( AimPlayer* data, LagRecord* record ) {

	// get predicted away angle for the player.
	float away = GetAwayAngle( record );

	// pointer for easy access.
	LagRecordMove* move = &data->m_walk_record; 

	// no moving context was found.
	if( move->m_sim_time <= 0.f || ( move->m_origin - record->m_origin ).length_sqr( ) > 128.f ) 
		return ResolveStand2( data, record );

	float time_diff = record->m_sim_time - move->m_sim_time;
	float diff = math::NormalizedAngle( record->m_body - move->m_body );

	// only try this once.
	if( data->m_body_index <= 0 ) {

		// it has not been time for this first update yet.
		if( time_diff < 0.22f || std::abs( diff ) < 35.f )
		{
			// set angles to current LBY.
			record->m_eye_angles.y = record->m_body;

			// set resolve mode.
			record->m_mode = Modes::RESOLVE_BODY;
			return;
		}
	}

	// clear prefer stand.
	data->m_prefer_stand.clear( );

	if( std::abs( time_diff ) > 1.3f ) {

		// LBY SHOULD HAVE UPDATED HERE.
		if( record->m_anim_time > data->m_body_update ) {
			// only shoot the LBY flick 3 times.
			// if we happen to miss then we most likely mispredicted.
			if( data->m_body_index <= 3 ) {
				data->m_prefer_stand.push_front( record->m_body );
				record->m_eye_angles.y = record->m_body;
				record->m_mode = Modes::RESOLVE_BODY;
				data->m_body_update = record->m_anim_time + 1.1f;
				return;
			}
		}

		// ok, no fucking update. apply big resolver.
		ResolveStand2( data, record );
		return;
	}

	if( !data->m_stored_first )
	{
		data->m_stored_diff  = diff;
		data->m_stored_first = true;
	}

	if( record->m_lag <= 1 || !data->m_stored_first || ( move->m_origin - record->m_origin ).length_sqr( ) < 16.f || data->m_body_index > 0 )
	{
		record->m_mode = Modes::RESOLVE_STAND1;
		record->m_eye_angles.y = move->m_body;

		int act = data->m_player->GetSequenceActivity( record->m_layers[ 3 ].m_sequence );

		// only shoot at last moving once.
		if( data->m_stand_index == 0 && ( std::abs( diff ) < 90.f || act == 980 ) ) {
			record->m_eye_angles.y = move->m_body;
			return;
		}

		// we missed 2 shots.
		if ( data->m_stand_index <= 2 ) {
			// try backwards.
			record->m_eye_angles.y = away + 180.f;
		}
		// jesus we can fucking stop missing can we?
		else {
			// lets just hope they switched ang after move.
			record->m_eye_angles.y = move->m_body + 180.f;
		}

		if( data->m_stand_index > 4 || act != 980 )
			ResolveStand2( data, record );

		return;
	}

	// big update.
	record->m_eye_angles.y = record->m_body;
	record->m_mode = Modes::RESOLVE_BODY;
	data->m_body_update = record->m_anim_time + 1.1f;
}

void Resolver::ResolveAir( AimPlayer* data, LagRecord* record ) {
 
	// we have barely any speed. 
	// either we jumped in place or we just left the ground.
	// or someone is trying to fool our resolver.
	if( record->m_velocity.length_2d( ) < 150.f ) {

		// invoke our stand resolver.
		record->m_eye_angles.y = record->m_body;

		// we are done.
		return;
	}

	// invalidate.
	data->m_walk_record = LagRecordMove{ };

	// try to predict the direction of the player based on his velocity direction.
	// this should be a rough estimation of where he is looking.
	float velyaw = math::rad_to_deg( std::atan2( record->m_velocity.y, record->m_velocity.x ) );

	// bruteforce.
	switch( data->m_missed_shots % 4 ) {
	case 0:
		record->m_eye_angles.y = record->m_body;
		break;

	case 1:
		record->m_eye_angles.y = velyaw - 180.f;
		break;

	case 2:
		record->m_eye_angles.y = velyaw - 90.f;
		break;

	case 3:
		record->m_eye_angles.y = velyaw + 90.f;
		break;
	}
}