/****************************************************************************/
/// @file    MSCACCLaneChanger.h
/// @author  Michele Segata
/// @date    Fri, 04 Mar 2012
/// @version $Id: MSCACCLaneChanger.h $
///
// Performs lane changing of vehicles, taking into account the presence of
// platooning vehicles and a dedicated lane for platooning
/****************************************************************************/
// SUMO, Simulation of Urban MObility; see http://sumo.sourceforge.net/
// Copyright (C) 2001-2011 DLR (http://www.dlr.de/) and contributors
/****************************************************************************/
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation; either version 2 of the License, or
//   (at your option) any later version.
//
/****************************************************************************/
// ===========================================================================
// included modules
// ===========================================================================
#ifdef _MSC_VER
#include <windows_config.h>
#else
#include <config.h>
#endif

#include "MSCACCLaneChanger.h"
#include "MSVehicle.h"
#include "MSVehicleType.h"
#include "MSVehicleTransfer.h"
#include "MSGlobals.h"
#include <cassert>
#include <iterator>
#include <cstdlib>
#include <cmath>
#include <microsim/MSAbstractLaneChangeModel.h>
#include <utils/common/MsgHandler.h>
#include <microsim/cfmodels/MSCFModel_CC.h>

#ifdef CHECK_MEMORY_LEAKS
#include <foreign/nvwa/debug_new.h>
#endif // CHECK_MEMORY_LEAKS

//#define DEBUG_VEHICLE_GUI_SELECTION 1
#ifdef DEBUG_VEHICLE_GUI_SELECTION
#include <utils/gui/div/GUIGlobalSelection.h>
#include <guisim/GUIVehicle.h>
#include <guisim/GUILane.h>
#endif

MSCACCLaneChanger::MSCACCLaneChanger(std::vector<MSLane*>* lanes, bool allowSwap) :
    MSLaneChanger(lanes, allowSwap) {
}

MSCACCLaneChanger::~MSCACCLaneChanger() {
}

enum CACC_ACTION {
    GOTO_LEFT, STAY_THERE, DONT_CARE
};

enum CACC_ACTION needtostayleft(MSVehicle* vehicle) {

    const MSCFModel_CC *model = dynamic_cast<const MSCFModel_CC *>(&vehicle->getCarFollowModel());

    //if the car is not CACC enabled, then we just let the default lane change model to take control
    if (!model)
        return DONT_CARE;

    if (MSNet::getInstance()->getCurrentTimeStep() <= 25000 || MSNet::getInstance()->getCurrentTimeStep() >= 60000) {
        return DONT_CARE;
    } else {
        if (vehicle->getLaneIndex() != 2) {
            return GOTO_LEFT;
        } else {
            return STAY_THERE;
        }
    }

}

bool MSCACCLaneChanger::change() {

    //the code has been copied and adapted from MSLaneChanger::change()

    // Find change-candidate. If it is on an allowed lane, try to change
    // to the right (there is a rule in Germany that you have to change
    // to the right, unless you are overtaking). If change to the right
    // isn't possible, check if there is a possibility to overtake (on the
    // left.
    // If candidate isn't on an allowed lane, changing to an allowed has
    // priority.
    myCandi = findCandidate();
    MSVehicle* vehicle = veh(myCandi);
#ifdef DEBUG_VEHICLE_GUI_SELECTION
    if (gSelected.isSelected(GLO_VEHICLE, static_cast<const GUIVehicle*>(vehicle)->getGlID())) {
        int bla = 0;
    }
#endif
    const std::vector<MSVehicle::LaneQ> &preb = vehicle->getBestLanes();
    assert(preb.size() == myChanger.size());
    for (int i = 0; i < (int) myChanger.size(); ++i) {
        ((std::vector<MSVehicle::LaneQ>&) preb)[i].occupation = myChanger[i].dens + preb[i].nextOccupation;
    }

    vehicle->getLaneChangeModel().prepareStep();
    std::pair<MSVehicle* const, SUMOReal> leader = getRealThisLeader(myCandi);

    enum CACC_ACTION cacc_goto_left = needtostayleft(vehicle);

    // check whether the vehicle wants and is able to change to left lane
    int state2 = 0;
    if (cacc_goto_left != STAY_THERE && (myCandi + 1) != myChanger.end() && (myCandi + 1)->lane->allowsVehicleClass(veh(myCandi)->getVehicleType().getVehicleClass())) {
        std::pair<MSVehicle* const, SUMOReal> lLead = getRealLeader(myCandi + 1);
        std::pair<MSVehicle* const, SUMOReal> lFollow = getRealFollower(myCandi + 1);
        state2 = cacc_goto_left == GOTO_LEFT ? LCA_SPEEDGAIN : 0;
        if ((state2 & LCA_URGENT) != 0 || (state2 & LCA_SPEEDGAIN) != 0) {
            state2 |= LCA_LEFT;
        }
        bool changingAllowed2 = (state2 & LCA_BLOCKED) == 0;
        //vehicle->getLaneChangeModel().setOwnState(state2|state1);
        // change if the vehicle wants to and is allowed to change
        if ((state2 & LCA_LEFT) != 0 && changingAllowed2) {
#ifndef NO_TRACI
            // inform lane change model about this change
            vehicle->getLaneChangeModel().fulfillChangeRequest(MSVehicle::REQUEST_LEFT);
#endif
            (myCandi + 1)->hoppedVeh = veh(myCandi);
            (myCandi + 1)->lane->myTmpVehicles.push_front(veh(myCandi));
            vehicle->leaveLane(MSMoveReminder::NOTIFICATION_LANE_CHANGE);
            myCandi->lane->leftByLaneChange(vehicle);
            vehicle->enterLaneAtLaneChange((myCandi + 1)->lane);
            (myCandi + 1)->lane->enteredByLaneChange(vehicle);
            vehicle->myLastLaneChangeOffset = 0;
            vehicle->getLaneChangeModel().changed();
            (myCandi + 1)->dens += (myCandi + 1)->hoppedVeh->getVehicleType().getLengthWithGap();
            return true;
        }
        if ((state2 & LCA_LEFT) != 0 && (state2 & LCA_URGENT) != 0) {
            (myCandi + 1)->lastBlocked = vehicle;
        }
    }
    vehicle->getLaneChangeModel().setOwnState(state2);

    // check whether the vehicles should be swapped
    if (myAllowsSwap && (state2 & (LCA_URGENT)) != 0) {
        // get the direction ...
        ChangerIt target;
        int dir;
        if ((state2 & (LCA_URGENT)) != 0) {
            // ... wants to go left
            target = myCandi + 1;
            dir = 1;
        }
        MSVehicle* prohibitor = target->lead;
        if (target->hoppedVeh != 0) {
            SUMOReal hoppedPos = target->hoppedVeh->getPositionOnLane();
            if (prohibitor == 0 || (hoppedPos > vehicle->getPositionOnLane() && prohibitor->getPositionOnLane() > hoppedPos)) {
                prohibitor = 0; // !!! vehicles should not jump over more than one lanetarget->hoppedVeh;
            }
        }
        if (prohibitor != 0 && ((prohibitor->getLaneChangeModel().getOwnState() & (LCA_URGENT/*|LCA_SPEEDGAIN*/)) != 0 && (prohibitor->getLaneChangeModel().getOwnState() & (LCA_LEFT | LCA_RIGHT)) != (vehicle->getLaneChangeModel().getOwnState() & (LCA_LEFT | LCA_RIGHT)))) {

            // check for position and speed
            if (prohibitor->getVehicleType().getLengthWithGap() - vehicle->getVehicleType().getLengthWithGap() == 0) {
                // ok, may be swapped
                // remove vehicle to swap with
                MSLane::VehCont::iterator i = find(target->lane->myTmpVehicles.begin(), target->lane->myTmpVehicles.end(), prohibitor);
                if (i != target->lane->myTmpVehicles.end()) {
                    MSVehicle* bla = *i;
                    assert(bla == prohibitor);
                    target->lane->myTmpVehicles.erase(i);
                    // set this vehicle
                    target->hoppedVeh = vehicle;
                    target->lane->myTmpVehicles.push_front(vehicle);
                    myCandi->hoppedVeh = prohibitor;
                    myCandi->lane->myTmpVehicles.push_front(prohibitor);

                    // leave lane and detectors
                    vehicle->leaveLane(MSMoveReminder::NOTIFICATION_LANE_CHANGE);
                    prohibitor->leaveLane(MSMoveReminder::NOTIFICATION_LANE_CHANGE);
                    // patch position and speed
                    SUMOReal p1 = vehicle->getPositionOnLane();
                    vehicle->myState.myPos = prohibitor->myState.myPos;
                    prohibitor->myState.myPos = p1;
                    p1 = vehicle->getSpeed();
                    vehicle->myState.mySpeed = prohibitor->myState.mySpeed;
                    prohibitor->myState.mySpeed = p1;
                    // enter lane and detectors
                    vehicle->enterLaneAtLaneChange(target->lane);
                    prohibitor->enterLaneAtLaneChange(myCandi->lane);
                    // mark lane change
                    vehicle->getLaneChangeModel().changed();
                    vehicle->myLastLaneChangeOffset = 0;
                    prohibitor->getLaneChangeModel().changed();
                    prohibitor->myLastLaneChangeOffset = 0;
                    (myCandi)->dens += prohibitor->getVehicleType().getLengthWithGap();
                    (target)->dens += vehicle->getVehicleType().getLengthWithGap();
                    return true;
                }
            }
        }
    }

    //if the car must stay in the reserved lane, then no lane change must be done
    if (cacc_goto_left == STAY_THERE) {
        // Candidate didn't change lane.
        myCandi->lane->myTmpVehicles.push_front(veh(myCandi));
        vehicle->myLastLaneChangeOffset += DELTA_T;
        (myCandi)->dens += vehicle->getVehicleType().getLengthWithGap();
        return false;
    }
    else {
        //if we get here, lane changing for platooning has not been done. invoke normal lane changing
        return MSLaneChanger::change();
    }

}

void
MSCACCLaneChanger::laneChange(SUMOTime t) {

    //the code has been copied and adapted from MSLaneChanger::laneChange()

    // This is what happens in one timestep. After initialization of the
    // changer, each vehicle will try to change. After that the changer
    // nedds an update to prevent multiple changes of one vehicle.
    // Finally, the change-result has to be given back to the lanes.
    initChanger();
    while (vehInChanger()) {

        bool haveChanged = change();
        updateChanger(haveChanged);
    }
    updateLanes(t);
}