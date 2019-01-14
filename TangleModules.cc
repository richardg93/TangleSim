#include <string>
#include <stdio.h>
#include <omnetpp.h>
#include "Tangle.h"
#include <fstream>

using namespace omnetpp;

enum MessageType { NEXT_TX_TIMER, POW_TIMER, TIP_REQUEST, ATTACH_CONFIRM };

//streams to write data to files
std::ofstream tipData;
std::ofstream blockWeightData;
std::ofstream tipAgeData;

std::vector<t_ptrTx> tracker;


/*
 * Classes for Transactors and the tangle network
 * TxActor has a field for the tips it can currently see - which it will
 * request from the Tangle module before attaching
 */

class TxActorModule : public cSimpleModule {

private:
    int issueCount;
    TxActor self; // non omnetpp implmentation of transactor
    simtime_t powTime;



protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage * msg) override;
public:
    std::vector<t_ptrTx> actorTipView; // tips sent by tangle are stored by transactor till they can approve some
    simtime_t tipTime; //time our tips view is from
};

Define_Module(TxActorModule);


class TangleModule : public cSimpleModule {

private:
    int txCount;
    int txLimit;
    Tangle tn;


protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage * msg) override;

};

Define_Module(TangleModule);


void TxActorModule::initialize() {

    cMessage * timer = new cMessage("nextTxTimer", NEXT_TX_TIMER );
    scheduleAt(simTime() + par("txGenRate"), timer);
    EV << "Starting next transaction procedure" << std::endl;
    powTime = par("powTime");

}

void TxActorModule::handleMessage(cMessage * msg) {

    if( msg->isSelfMessage() ){

        if( msg->getKind() == NEXT_TX_TIMER ) { //SELF MESSAGE TO START TRANSACTION PROCEDURE AGAIN

            delete msg;
            //send request to tangle for tips

            EV << "TxActor " << getId() << ": requesting tips from tangle" << std::endl;
            cMessage * tipRequest = new cMessage("tipRequest", TIP_REQUEST);
            send(tipRequest, "tangleConnect$o");



        } else { //POW_TIMER

            EV << "TxActor " << getId() << ": POW completed, approving tips" << std::endl;
            issueCount++;
            delete msg;
            //store number of tips seen by transaction before and add to confirmation message


            EV << "Tips seen before starting POW: " << actorTipView.size() << std::endl;


            /*
             * WALK TIP SELECTION TESTING CODE
             */

            t_ptrTx testTx = self.getWalkStart( actorTipView, par("walkDepth") );
            EV << "Backtrack found Tx: " << testTx << " with weight: " << self.ComputeWeight(testTx, simTime()) << std::endl;


            /*
             * END OF WALK TIP SELECTION TEST CODE
             */

            if( strcmp( par("tipSelectionMethod"), "URTS") == 0 ) {

                t_txApproved chosenTips = self.URTipSelection(actorTipView);
                self.attach( actorTipView, simTime(), chosenTips, true);

            } else { //WALK

                t_txApproved chosenTips;

                for(int i = 0; i < APPROVE_VAL; i++) {
                    //get start point for each walk
                    t_ptrTx walkStart = self.getWalkStart( actorTipView, par("walkDepth") );

                    //find a tip
                    chosenTips[i] = self.WalkTipSelection( walkStart, par("walkAlphaValue"), actorTipView, tipTime );
                }
                bool kind = true;
                if(powTime > 1) kind = false;
                self.attach( actorTipView, tipTime, chosenTips, kind);
            }


            EV << "Actual tips after: " << self.getTanglePtr()->giveTips().size() << std::endl;
            EV << "Approved Tx #" << self.getMyTx().back()->m_TxApproved[0]->TxNumber << " and Tx #" << self.getMyTx().back()->m_TxApproved[1] << std::endl;

            //start a new issue timer
            cMessage * timer = new cMessage("requestTimer");
            scheduleAt(simTime() + par("txGenRate"), timer);

            //Inform tangle of attached Tx
            cMessage * attachConfirm = new cMessage("attachConfirmed", ATTACH_CONFIRM);

            //Tangle knows which tx was just attached from message context pointer
            attachConfirm->setContextPointer( self.getMyTx().back().get() );
            send(attachConfirm, "tangleConnect$o");

            //log data for new tx in .txt file
            std::string data;

            //tx Number
            data.append(std::to_string(self.getMyTx().back()->TxNumber));
            data.push_back(',');

            //tip count before
            data.append( std::to_string(actorTipView.size()) );
            data.push_back(',');

            //tip count after
            data.append( std::to_string(self.getTanglePtr()->getTipNumber()) );
            data.push_back('\n');
            tipData << data;

            if(self.getMyTx().back()->TxNumber % 10 == 0) {
                tracker.push_back(self.getMyTx().back());
            }

            //append weights of transactions to data file to track how they change
            if(self.getMyTx().back()->TxNumber % 100 == 0) {
                if(tracker.size() != 0) {
                    for(int i =0; i < tracker.size(); i++) {
                        std::string txType;
                        if(tracker[i]->normalLatency) txType = "low";
                        else txType = "high";
                        blockWeightData << tracker[i]->TxNumber << "," << self.ComputeWeight(tracker[i], simTime()) << "," << txType << std::endl;

                    }
                }
            }
        }

    } else { //TIP MESSAGE FROM TANGLE

            // give the transactors a pointer to the tangle they're interacting with

            if(self.getTanglePtr() == nullptr) {
                self.setTanglePtr( (Tangle *) msg->getContextPointer() );
            }

            //get copy of current tips
            actorTipView = self.getTanglePtr()->giveTips();
            tipTime = simTime();

            //start timer for when POW is completed
            cMessage * powTimer = new cMessage("powTimer", POW_TIMER);
            scheduleAt(simTime() + powTime, powTimer);

            delete msg;
    }
}

void TangleModule::initialize() {
    tracker.clear();
    txCount = 0;
    txLimit = par("transactionLimit");
    Tx::tx_totalCount = 0;
    //NEED TO CHANGE THE TXT FILENAME FOR DIFFERENT SIMULATIONS
    std::string filename = par("dataFilename");
    tipData.open(filename.c_str(), std::ios::app);
    tipData << "TxNumber," << "Tips seen," << "Tips after" << std::endl;
    std::string filename2 = par("tipAgeFilename");
    tipAgeData.open(filename2.c_str(), std::ios::app);
    tipAgeData << "TxNumber," << "Tip Age," << "First Approval Time," << "Attach Time," << "Direct Approvers" << std::endl;
    std::string filename3 = par("blockWeightFilename");
    blockWeightData.open(filename3.c_str(), std::ios::app);
    blockWeightData << "TxNumber," << "Weight," << "kind" << std::endl;



}

void TangleModule::handleMessage(cMessage * msg) {

    if(msg->getKind() == TIP_REQUEST) {

        int arrivalGateIndex = msg->getArrivalGate()->getIndex();

        EV << "Tip request from TxActor " << msg->getSenderModuleId() << std::endl;
        EV << "Total tips at time " << simTime() << ": " << tn.giveTips().size() << std::endl;
        txCount++;

        cMessage * tipMessage = new cMessage("tipMessage");

        tipMessage->setContextPointer(&tn); //so transactor can access Tangle methods

        send(tipMessage, "actorConnect$o", arrivalGateIndex);
        delete msg;

    } else if(msg->getKind() == ATTACH_CONFIRM){

            Tx * justAttached = (Tx *) msg->getContextPointer();

            if( justAttached->TxNumber >= txLimit ){

                EV << "Transaction Limit reached, stopping simulation" << std::endl;
                delete msg;

                tipData.close();

                //record time from attach to first approval
                //excludes genesisblock
                double tipAge;
                for(int i = 0; i < tn.allTx.size(); i++) {

                    tipAge = tn.allTx[i]->firstApprovedTime.dbl() - tn.allTx[i]->timeStamp.dbl();
                    tipAgeData << tn.allTx[i]->TxNumber << "," << tipAge << "," << tn.allTx[i]->firstApprovedTime.dbl() << "," << tn.allTx[i]->timeStamp.dbl() << "," << tn.allTx[i]->m_approvedBy.size() << std::endl;

                }
                tipAgeData.close();
                blockWeightData.close();
                endSimulation();

            } else {

                EV << "Total Transactions now: " << justAttached->TxNumber << std::endl;
                delete msg;
            }
    }

}




