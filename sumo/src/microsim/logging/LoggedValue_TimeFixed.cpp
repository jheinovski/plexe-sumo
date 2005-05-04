//---------------------------------------------------------------------------//
//                        LoggedValue_TimeFixed.cpp -
//  This class logs values over a previously defined, fixed time
//                           -------------------
//  project              : SUMO - Simulation of Urban MObility
//  begin                : Sept 2002
//  copyright            : (C) 2002 by Daniel Krajzewicz
//  organisation         : IVF/DLR http://ivf.dlr.de
//  email                : Daniel.Krajzewicz@dlr.de
//---------------------------------------------------------------------------//

//---------------------------------------------------------------------------//
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation; either version 2 of the License, or
//   (at your option) any later version.
//
//---------------------------------------------------------------------------//
/*
namespace
{
    const char rcsid[] =
    "$Id$";
}
*/
// $Log$
// Revision 1.5  2005/05/04 08:09:23  dkrajzew
// level 3 warnings removed; a certain SUMOTime time description added
//
// Revision 1.4  2004/03/19 13:06:44  dkrajzew
// some work on the style
//
// Revision 1.3  2004/03/01 11:15:12  roessel
// Reintroduced formerly removed files.
//
// Revision 1.1  2003/11/11 08:07:37  dkrajzew
// logging (value passing) moved from utils to microsim
//
// Revision 1.3  2003/06/05 14:29:39  dkrajzew
// class templates applied; documentation added
//
/* =========================================================================
 * included modules
 * ======================================================================= */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif // HAVE_CONFIG_H


#include "LoggedValue.h"
#include "LoggedValue_TimeFixed.h"

template<typename _T>
LoggedValue_TimeFixed<_T>::LoggedValue_TimeFixed<_T>(size_t sampleInterval)
    : LoggedValue<_T>(sampleInterval),
    mySampleInterval(sampleInterval),
    mySampledUnits(0)
{
}


template<typename _T>
LoggedValue_TimeFixed<_T>::~LoggedValue_TimeFixed<_T>()
{
}


template<typename _T>
void
LoggedValue_TimeFixed<_T>::add(_T value)
{
    mySampledUnits++;
    // clear on a new interval
    if(mySampledUnits>mySampleInterval) {
        mySampledUnits = 1;
        myCurrentValue = 0;
    }
    // add new value
    myCurrentValue += value;
}


template<typename _T>
_T
LoggedValue_TimeFixed<_T>::getAvg() const
{
    // Security check for false interval usage
    if(mySampledUnits==0) {
        return 0; // !!! You could also throw an exception here;
        // This is not meant to be done
    }
    return myCurrentValue / (double) mySampledUnits;
}


template<typename _T>
_T
LoggedValue_TimeFixed<_T>::getAbs() const
{
    return myCurrentValue;
}




/**************** DO NOT DEFINE ANYTHING AFTER THE INCLUDE *****************/

// Local Variables:
// mode:C++
// End:


