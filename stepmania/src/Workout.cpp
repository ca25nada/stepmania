#include "global.h"
#include "Workout.h"
#include "EnumHelper.h"
#include "LocalizedString.h"
#include "RageUtil.h"
#include "LuaManager.h"
#include "XmlFile.h"
#include "XmlFileUtil.h"
#include "Course.h"
#include "Foreach.h"

static const char *WorkoutProgramNames[] = {
	"FatBurn",
	"Interval",
	"Runner",
	"Flat",
};
XToString( WorkoutProgram );
StringToX( WorkoutProgram );
XToLocalizedString( WorkoutProgram );


Workout::Workout()
{
	m_bNameWasSetByUser = false;
	m_WorkoutProgram = (WorkoutProgram)0;
	m_iMinutes = 10;
	m_iAverageMeter = 3;
}

int Workout::GetEstimatedNumSongs() const
{
	return GetEstimatedNumSongsFromSeconds( m_iMinutes*60.0f );
}

int Workout::GetEstimatedNumSongsFromSeconds( float fSeconds )
{
	int iNumSongsInBody = (int)ceilf(fSeconds / (60+50));
	return iNumSongsInBody;
}

const float fAverageSongLengthSeconds = 105;
static int CalculateWorkoutProgramMeter( WorkoutProgram wp, int iAverageMeter, int iSongInBodyIndex, int iNumSongsInBody )
{
	const int iMaxMeter = iAverageMeter + 1;
	const int iMinMeter = iAverageMeter - 1;

	float fPercentThroughBody = SCALE( iSongInBodyIndex, 0.0f, (float)iNumSongsInBody-1, 0.0f, 1.0f );
	CLAMP( fPercentThroughBody, 0.0f, 1.0f );

	int iMeter = -1;	// fill this in

	switch( wp )
	{
	DEFAULT_FAIL( wp );
	case WorkoutProgram_FatBurn:
		{
			const int iSongInterval = 3;
			float fMeter = SCALE( iSongInBodyIndex % iSongInterval, 0.0f, (float)(iSongInterval-1), (float)iMinMeter, (float)iMaxMeter );
			iMeter = lrintf( fMeter );
		}
		break;
	case WorkoutProgram_Interval:
		{
			iMeter = (iSongInBodyIndex % 2) ? iMaxMeter:iMinMeter;
		}
		break;
	case WorkoutProgram_Runner:
		{
			const int iSongInterval = 6;
			float fMeter = SCALE( iSongInBodyIndex % (iSongInterval/2), 0.0f, (iSongInterval/2)-1.0f, (float)iMinMeter, (float)iMaxMeter );
			if( (iSongInBodyIndex % iSongInterval) >= iSongInterval/2 )
				fMeter = SCALE( fMeter, (float)iMinMeter, (float)iMaxMeter, (float)iMaxMeter, (float)iMinMeter );
			iMeter = lrintf( fMeter );
		}
		break;
	case WorkoutProgram_Flat:
		{
			iMeter = iAverageMeter;
		}
		break;
	}

	CLAMP( iMeter, MIN_METER, MAX_METER );
	return iMeter;
}

void Workout::GetEstimatedMeters( int iNumSongs, vector<int> &viMetersOut )
{
	viMetersOut.clear();

	for( int i=0; i<iNumSongs; i++ )
	{
		int iMeter = CalculateWorkoutProgramMeter( m_WorkoutProgram, m_iAverageMeter, i, iNumSongs );
		viMetersOut.push_back( iMeter );
	}
}

void Workout::GenerateCourse( Course &out )
{
	out = Course();

	out.m_sMainTitle = "temp";
	out.m_bRepeat = true;

	FOREACH( SongID, m_vSongs, s )
	{
		CourseEntry ce;
		ce.songID = *s;
		out.m_vEntries.push_back( ce );
	}
}

bool Workout::LoadFromFile( RString sFile )
{
	XNode xml;
	XmlFileUtil::LoadFromFileShowErrors( xml, sFile );
	if( xml.GetName() != "Workout" )
		return false;

	m_sFile = sFile;

	xml.GetChildValue("Name",m_sName);
	m_bNameWasSetByUser = true;
	// XXX: ValidateWorkoutName

	RString s;
	xml.GetChildValue("WorkoutProgram",s);
	m_WorkoutProgram = StringToWorkoutProgram( s );
	if( m_WorkoutProgram == WorkoutProgram_Invalid )
		m_WorkoutProgram = (WorkoutProgram)0;

	xml.GetChildValue("Minutes",m_iMinutes);
	CLAMP( m_iAverageMeter, MIN_WORKOUT_MINUTES, MAX_WORKOUT_MINUTES );

	xml.GetChildValue("AverageMeter",m_iAverageMeter);
	CLAMP( m_iAverageMeter, MIN_METER, MAX_METER );

	XNode *songs = xml.GetChild("Songs");
	if( songs )
	{
		FOREACH_CONST_Child( songs, song )
		{
			if( song->GetName() == "Song" )
			{
				RString s;
				song->GetTextValue( s );
				SongID sid;
				sid.FromString( s );
				m_vSongs.push_back( sid );
			}
		}
	}
	return true;
}

bool Workout::SaveToFile( RString sFile )
{
	m_sFile = sFile;

	XNode xml( "Workout" );
	xml.AppendChild( "Name", m_sName );
	RString s;
	xml.AppendChild( "WorkoutProgram", WorkoutProgramToString(m_WorkoutProgram) );
	xml.AppendChild( "Minutes", m_iMinutes );
	xml.AppendChild( "AverageMeter", m_iAverageMeter );
	
	XNode *songs = xml.AppendChild("Songs");
	FOREACH_CONST( SongID, m_vSongs, s )
	{
		songs->AppendChild( "Song", s->ToString() );
	}

	return XmlFileUtil::SaveToFile( &xml, sFile );
}

// lua start
#include "LuaBinding.h"

class LunaWorkout: public Luna<Workout>
{
public:
	DEFINE_METHOD( GetName,			m_sName )
	DEFINE_METHOD( GetMinutes,		m_iMinutes )
	DEFINE_METHOD( GetEstimatedNumSongs,	GetEstimatedNumSongs() )

	LunaWorkout()
	{
		ADD_METHOD( GetName );
		ADD_METHOD( GetMinutes );
		ADD_METHOD( GetEstimatedNumSongs );
	}
};

LUA_REGISTER_CLASS( Workout )
// lua end

/*
 * (c) 2003-2004 Chris Danford
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, provided that the above
 * copyright notice(s) and this permission notice appear in all copies of
 * the Software and that both the above copyright notice(s) and this
 * permission notice appear in supporting documentation.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
 * THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS
 * INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
