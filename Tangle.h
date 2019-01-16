#pragma once
#include <array>
#include <vector>
#include <memory>
#include <random>
#include <ctime>
#include <omnetpp.h>


struct Tx;
class Tangle;
class TxActor;

const unsigned int APPROVE_VAL = 2;

using t_ptrTx = std::shared_ptr<Tx>;
using t_txApproved = std::array<t_ptrTx, APPROVE_VAL>;

struct Tx
{

	std::vector<t_ptrTx> m_approvedBy;
	t_txApproved m_TxApproved;
	TxActor * m_issuedBy;
	omnetpp::simtime_t timeStamp; //set by TxActor on intialisation in TxActor::Attach
	omnetpp::simtime_t firstApprovedTime;
	bool isGenesisBlock = false;
	bool normalLatency = true;

	//debug
	bool isApproved = false;
	static long int tx_totalCount;
	long int TxNumber;
	bool isVisited = false;
	bool hasApprovees();

	Tx();
};

class Tangle
{

    private:
        std::vector<t_ptrTx> m_tips;
        t_ptrTx m_genesisBlock;

        //generator in Tangle as creating separate gen's for each actor that acted differently proved hard
        //i appreciate this entire simulation needs refactoring

        std::mt19937 tipSelectGen;

    public:
        Tangle();
        int walkDepth;
        std::vector<t_ptrTx> allTx;

        //debug
        static int TangleGiveTipsCount;
        std::vector<t_ptrTx> giveTips();
        void ReconcileTips(const t_txApproved& removeTips);
        void addTip(t_ptrTx newTip);
        std::mt19937& getRandGen();
        int getTipNumber();
        const t_ptrTx& giveGenBlock() const;
        std::vector<t_ptrTx>& getTracker();

};



class TxActor
{

    private:
        std::vector<t_ptrTx> m_MyTx;
        Tangle * tanglePtr = nullptr;

        //recursive func to compute cumulative weight of a transaction, called from public func ComputeWeight
        int _computeWeight( std::vector<t_ptrTx>& visited, t_ptrTx current, omnetpp::simtime_t timeStamp );

    public:
        TxActor();
        std::shared_ptr<TxActor> getptr();

        t_txApproved URTipSelection( std::vector<t_ptrTx> tips );
        void attach( std::vector<t_ptrTx>& storedTips, omnetpp::simtime_t attachTime, t_txApproved& chosen );


        //returns a tip to approve via a walk - randomness determined by param
        t_ptrTx WalkTipSelection( t_ptrTx start, double alphaVal, std::vector<t_ptrTx>& tips, omnetpp::simtime_t timeStamp );

        Tangle* getTanglePtr() const;
        void setTanglePtr( Tangle* tn );
        const std::vector<t_ptrTx>& getMyTx() const;

        //computes cumulative weight of any given transaction - used heavily in walk tip selection
        //indirect recursion
        int ComputeWeight( t_ptrTx tx, omnetpp::simtime_t timeStamp );

        //backtrack a determined distance in the tangle to find a start point for a random walk
        t_ptrTx getWalkStart( std::vector<t_ptrTx>& tips, int backTrackDist );

        //checks if TxActor sees the tx its walker is on as a tip
        bool isRelativeTip( t_ptrTx toCheck, std::vector<t_ptrTx>& tips );

        void filterView( std::vector<t_ptrTx>& view, omnetpp::simtime_t timeStamp );

        //Returns the index of the heaviest tx in the actors tip view
       int findMaxWeightIndex(std::vector<t_ptrTx>& view, omnetpp::simtime_t timeStamp );

        static int actorCount;

};

