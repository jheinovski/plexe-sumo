/***************************************************************************
                          NIVissimSingleTypeParser_Zuflussdefinition.cpp

                             -------------------
    begin                : Wed, 18 Dec 2002
    copyright            : (C) 2001 by DLR/IVF http://ivf.dlr.de/
    author               : Daniel Krajzewicz
    email                : Daniel.Krajzewicz@dlr.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
namespace
{
    const char rcsid[] =
    "$Id$";
}
// $Log$
// Revision 1.2  2003/05/20 09:42:38  dkrajzew
// all data types implemented
//
// Revision 1.1  2003/02/07 11:08:43  dkrajzew
// Vissim import added (preview)
//
//

/* =========================================================================
 * included modules
 * ======================================================================= */
#include <iostream>
#include <utils/convert/TplConvert.h>
#include "../NIVissimLoader.h"
#include "../tempstructs/NIVissimSource.h"
#include "NIVissimSingleTypeParser_Zuflussdefinition.h"


/* =========================================================================
 * used namespaces
 * ======================================================================= */
using namespace std;


/* =========================================================================
 * method definitions
 * ======================================================================= */
NIVissimSingleTypeParser_Zuflussdefinition::NIVissimSingleTypeParser_Zuflussdefinition(NIVissimLoader &parent)
	: NIVissimLoader::VissimSingleTypeParser(parent)
{
}


NIVissimSingleTypeParser_Zuflussdefinition::~NIVissimSingleTypeParser_Zuflussdefinition()
{
}


bool
NIVissimSingleTypeParser_Zuflussdefinition::parse(std::istream &from)
{
	string id, edgeid;
    from >> id; // type-checking is missing!
    string tag, name;
    // override some optional values till q
    while(tag!="q") {
        tag = overrideOptionalLabel(from);
        if(tag=="name") {
            name = readName(from);
        } else if(tag=="strecke") {
            from >> edgeid; // type-checking is missing!
        }
    }
    // read q
    bool exact = false;
    double q;
    tag = myRead(from);
    if(tag=="exakt") {
        exact = true;
        tag = myRead(from);
    }
    q = TplConvert<char>::_2float(tag.c_str());
    // read the vehicle types
    from >> tag;
    int vehicle_combination;
    from >> vehicle_combination;
    // check whether optional time information is available
    tag = readEndSecure(from);
    double beg, end;
    beg = -1;
    end = -1;
    if(tag=="zeit") {
        from >> tag;
        from >> beg;
        from >> tag;
        from >> end;
    }
    return NIVissimSource::dictionary(id, name, edgeid, q, exact, vehicle_combination,
        beg, end);
}

