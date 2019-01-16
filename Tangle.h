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
    //Other transactions that have approved this transaction
	std::vector<t_ptrTx> m_approvedBy;

	//Transactions approved by this transactions
	t_txApproved m_TxApproved;

	//The transactor that issued this transaction
	TxActor * m_issuedBy;

	//Time that the transaction was issued - set by TxActor on intialisation in TxActor::Attach
	omnetpp::simtime_t timeStamp;

	//Time this transaction ceased to be a tip
	omnetpp::simtime_t firstApprovedTime;

	bool isGenesisBlock = false;
	bool isApproved = false;

	// Only used by ComputeWeight and _computeWeight, set when the recursion visits this transaction
	// reset to false in ComputeWeight
	bool isVisited = false;

	bool hasApprovees();

	// Keep track of how many transactions have been created, use this number on construction to set
	// an identifier per transaction
	static long int tx_totalCount;
	long int TxNumber;

	//Define constructor to set TxNumber - no other reason, otherwise POD
	Tx();

};

class Tangle
{
        //TODO: Make singleton

    private:
        // Keep a record of all the current unapproved transactions
        std::vector<t_ptrTx> m_tips;

        // The first transaction - initialised on construction
        t_ptrTx m_genesisBlock;

        // RNG in tangle to simplify tip selection
        //TODO: Reimplement to take as a param from omnet ned file
        std::mt19937 tipSelectGen;

    public:
        Tangle();

        // Determines how far back a transactor will look into the tangle for a start point, from which they will
        // move out to a tip to approve (tip selection)
        //TODO: Move this to TXactor - can then be configurable per transactor
        int walkDepth;

        //All transactions
        std::vector<t_ptrTx> allTx;

        // Returns a copy of the current tips from the Tangle (Needs to be a copy to simulate an asynchronous view of the tangle per transactor)
        std::vector<t_ptrTx> giveTips();

        // Compares the tips just approved ( removeTips ) with the tangles tip view, if it finds any references to the tips just approved
        // it will remove them from the tangle's view
        void ReconcileTips(const t_txApproved& removeTips);

        // Newly issued transaction is added to the list of unconfirmed transactions
        void addTip(t_ptrTx newTip);

        // Returns ref to the RNG, used in all TxActor methods
        // TODO: Needs refactoring, perhaps a static RNG for each use in TxActor?
        std::mt19937& getRandGen();

        // Return current number of unapproved transactions
        int getTipNumber();

        // Returns a reference to the first transaction
        const t_ptrTx& giveGenBlock() const;

        // Debug
        static int TangleGiveTipsCount;

};



class TxActor
{

    private:
        // All the transactions this transacotr has issued
        std::vector<t_ptrTx> m_MyTx;
        Tangle * tanglePtr = nullptr;

        // Recursive func to compute cumulative weight of a transaction, called from public func ComputeWeight
        int _computeWeight( std::vector<t_ptrTx>& visited, t_ptrTx current, omnetpp::simtime_t timeStamp );

    public:
        TxActor();
        // Tip selection method that picks uniformly between all the tips in the transactors view
        t_txApproved URTipSelection( std::vector<t_ptrTx> tips );

        // Uses internal reference to Tangle object to approve transactions it has chosen via a tip selection methpod, then maks sure the Tangle
        // object is has a reference to has update it's tip view
        //TODO: Potential for a static method in a hitherto undefined Tangle namespace instead of a member
        void attach( std::vector<t_ptrTx>& storedTips, omnetpp::simtime_t attachTime, t_txApproved& chosen );

        //returns a tip to approve via a walk - randomness determined by param
        t_ptrTx WalkTipSelection( t_ptrTx start, double alphaVal, std::vector<t_ptrTx>& tips, omnetpp::simtime_t timeStamp );

        // Return the pointer to the Tangle object this transactor is referring to
        // see todo in Tangle
        Tangle* getTanglePtr() const;

        void setTanglePtr( Tangle* tn );

        //Returns a reference to all the transactions this transaction has issued
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

