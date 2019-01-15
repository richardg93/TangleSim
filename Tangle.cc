#include "Tangle.h"
#include <iostream>
#include <random>
#include <chrono>
#include <algorithm>


int TxActor::actorCount;
long int Tx::tx_totalCount;
int Tangle::TangleGiveTipsCount;

/*
    Tx DEFINITIONS
*/

//return true if Tx has approved at least one other transactions
bool Tx::hasApprovees()
{
    int approveeCount = 0;

    for( int i = 0; i < APPROVE_VAL; ++i )
    {

        if( m_TxApproved.at( i ) )
        {
            approveeCount++;
        }

    }

    if( approveeCount > 0 )
    {
        return true;
    }
    else
    {
        return false;
    }

}

//Tx def END

/*
    TANGLE DEFINITIONS
*/

Tangle::Tangle() try : m_genesisBlock( new Tx )
{
     m_tips.push_back( m_genesisBlock );
     unsigned tipSelectSeed;
     tipSelectSeed = std::chrono::system_clock::now().time_since_epoch().count(); //needs to use seeds from omnetpp
     tipSelectGen.seed( tipSelectSeed );
     m_genesisBlock->isGenesisBlock = true;
}

catch ( std::bad_alloc e)
{
     std::cerr << e.what() << std::endl;
     std::cerr << "Failed to create genesis transaction" << std::endl;
}

//method to return a copy of all the current unconfirmed transactions
std::vector<t_ptrTx> Tangle::giveTips()
{
     return m_tips;
}

//checks the newly approved tips against the current tip vecotr held by tangle
//removes any that are still unconfirmed
void Tangle::ReconcileTips( const t_txApproved& removeTips )
{


    for ( int i = 0; i < APPROVE_VAL; ++i )
    {

        for ( int j = 0; j < m_tips.size(); ++j )
        {

            if ( m_tips.at( j ) == removeTips.at( i ) )
            {
                m_tips.erase( m_tips.begin() + j );
            }
        }
    }
}

//adds a pointer to a newly added but as yet unconfirmed Tx to the tip list
void Tangle::addTip( t_ptrTx newTip )
{
     m_tips.push_back( newTip );
     allTx.push_back( newTip );
}

std::mt19937& Tangle::getRandGen()
{
    return tipSelectGen;
}

int Tangle::getTipNumber()
{
    return m_tips.size();
}

const t_ptrTx& Tangle::giveGenBlock() const
{
    return m_genesisBlock;
}

//IGNORE FOR NOW
//returs reference to transaction we are tracking
std::vector<t_ptrTx>& Tangle::getTracker()
{
    return trackTx;
}

// Tangle def END

/*
    TXACTOR DEFINITIONS
*/

//Constructor

TxActor::TxActor() { ++actorCount; }

//Tips to approve selected completely at random
t_txApproved TxActor::URTipSelection( std::vector<t_ptrTx> tips )
{
     // tips passed by value to allow a transaction a view of the Tangle as it started computing proof of work
     t_txApproved chosenTips;

     int index;

     for ( int i = 0; i < APPROVE_VAL; ++i )
     {
         if(tips.size() > 0)
         {
             std::uniform_int_distribution<int> tipDist( 0, tips.size() -1 );
             index = tipDist( getTanglePtr()->getRandGen() );

             chosenTips.at( i ) = tips.at( index );
             tips.erase( tips.begin() + index );

         }
     }
     return chosenTips;
}

//creates a new transaction, selects tips for it to approve, then adds the new transaction to the tip list
//ready for approval by the proceeding transactions
void TxActor::attach( std::vector<t_ptrTx>& storedTips, omnetpp::simtime_t attachTime, t_txApproved& chosen, bool kind )
{
     try
     {

         //create new tx
         m_MyTx.emplace_back( new Tx() );
         m_MyTx.back()->m_issuedBy = this;
         m_MyTx.back()->timeStamp = attachTime;

         if( kind == false )
         {
             m_MyTx.back()->normalLatency = false;
         }

         //add pointer to new Tx to tips selected, so they know who approved them
         for ( int i = 0; i < APPROVE_VAL; ++i )
         {

             if( chosen.at( i ) )
             {
                 // if only one tip available (at start) will only be one actual tx approved - throws seg fault if not accounted for
                 chosen.at( i )->m_approvedBy.push_back( m_MyTx.back() );

                 if( !chosen.at( i )->isApproved )
                 {
                     //with firstApprovedTime and timeAttached as field - we can compute the age of a transaction
                     chosen.at( i )->firstApprovedTime = attachTime;
                 }

                 chosen.at( i )->isApproved = true;
             }

         }

         for( int i = 0; i < APPROVE_VAL; ++i )
         {
             //approve tips
             if( chosen.at( i ) )
             {
                 m_MyTx.back()->m_TxApproved.at( i ) = chosen.at( i );
             }

         }

         //remove pointers to tips just approved, from tips vector in tangle
         getTanglePtr()->ReconcileTips( chosen );

         //add newly created Tx to Tangle tips list
         getTanglePtr()->addTip( m_MyTx.back() );

     }
     catch ( std::bad_alloc& e )
     {
         std::cerr << e.what() << std::endl;
         std::cerr << "Not enough memory for TxActor to produce a new Tx" << std::endl;
     }

}

//getter and setter for tangle pointer, makes it easy to interact with the tips
 Tangle* TxActor::getTanglePtr() const
{
    return tanglePtr;
}

void TxActor::setTanglePtr( Tangle* tn )
{
    tanglePtr = tn;
}

const std::vector<t_ptrTx>& TxActor::getMyTx() const
{
    return m_MyTx;
}

//compute weight definitions

//indirect recursion, start point here - calls private recursive function _computeWeight
int TxActor::ComputeWeight( t_ptrTx tx, omnetpp::simtime_t timeStamp )
{

    std::vector<t_ptrTx> visited;
    int weight = _computeWeight( visited, tx, timeStamp );

    //leave txes as we found them
    for( int i = 0; i < visited.size(); ++i )
    {

        for( int j = 0; j < visited.at( i )->m_approvedBy.size(); ++j )
        {
            visited.at( i )->m_approvedBy.at( j )->isVisited = false;
        }

    }

    tx->isVisited = false;
    return weight + 1;

}

//traverses tangle returns weight of each transaction, stopping cases: previously visited transaction, transaction with
//a timestamp after TxActor started computing, and on reaching a tip
int TxActor::_computeWeight( std::vector<t_ptrTx>& visited, t_ptrTx current, omnetpp::simtime_t timeStamp )
{

    //could TxActor "see" the current Tx their walk is located on?
    if( timeStamp < current->timeStamp )
    {
        visited.push_back( current );
        current->isVisited = true;
        return 0;
    }

    //if we reach a tip
    if( current->m_approvedBy.size() == 0 )
    {
        visited.push_back( current );
        current->isVisited = true;

        return 0;
    }

    visited.push_back( current );

    current->isVisited = true;
    int weight = 0;

    for( int i = 0; i < current->m_approvedBy.size(); ++i )
    {

        if( !current->m_approvedBy.at( i )->isVisited )
        {
                //check if next tx has been visited before
                weight += 1 + _computeWeight( visited, current->m_approvedBy.at( i ), timeStamp );
        }

    }

    return weight;

}

t_ptrTx TxActor::getWalkStart( std::vector<t_ptrTx>& tips, int backTrackDist )
{

    std::uniform_int_distribution<int> tipDist( 0, tips.size() -1 );
    int index = tipDist( getTanglePtr()->getRandGen() );

    t_ptrTx current = tips.at( index );

    int count = backTrackDist;
    int approvesIndex;

    //start backtrack
    //go until genesis block or reach backtrack distance
    while( !current->isGenesisBlock && count > 0 )
    {

        std::uniform_int_distribution<int> choice( 0, APPROVE_VAL -1 );
        approvesIndex = choice( getTanglePtr()->getRandGen() ) ;

        if( !current->m_TxApproved[approvesIndex] )
        {
            current = current->m_TxApproved.at( 0 );
        }
        else
        {
            current = current->m_TxApproved.at( approvesIndex );
        }

        --count;

    }

    return current;

}

t_ptrTx TxActor::WalkTipSelection( t_ptrTx start, double alphaVal, std::vector<t_ptrTx>& tips, omnetpp::simtime_t timeStamp )
{

    std::vector<t_ptrTx> currentView; //copy of each transactions approvers

    //both used to determine the next Tx to walk to
    int walkCounts = 0;

    t_ptrTx current = start;

    //keep going until we reach a "tip" in relation to the view of the tangle that TxActor has
    while( !isRelativeTip( current, tips ) )
    {

        ++walkCounts;

        std::vector<t_ptrTx> currentView = current->m_approvedBy;

        //filter current View
        filterView( currentView, timeStamp );

        if( currentView.size() == 0 )
        {
            break;
        }

        //if only one approver available dont compute the weight
        if( currentView.size() == 1 )
        {
            current = currentView.front();

        }
        else
        {
            //if more than one find the max weight
            //get heaviest tx to simplify choosing next
            int maxWeightIndex = findMaxWeightIndex( currentView, timeStamp );
            t_ptrTx heaviestTx = currentView.at( maxWeightIndex );

            currentView.erase( currentView.begin() + maxWeightIndex );

            //if no more after heaviest removal, choose heaviest
            if( currentView.size() == 0 )
            {
                current = heaviestTx;

            }
            else
            {
                // if at least one non heaviest still available pick between them
                std::uniform_real_distribution<double> walkChoice( 0.0, 1.0 );

                if( walkChoice( getTanglePtr()->getRandGen() ) < alphaVal)
                {
                    current = heaviestTx;
                }
                else
                {
                    //otherwise choose a random site
                    if( currentView.size() == 1 )
                    {
                        //if only one left after removing heaviest use the remaining one
                        current = currentView.front();

                    }
                    else
                    {
                        //otherwise pick at random
                        std::uniform_int_distribution<int> siteChoice( 0, currentView.size() - 1 );
                        int choiceIndex = siteChoice( getTanglePtr()->getRandGen() );
                        current = currentView.at( choiceIndex );

                    }
                }
            }

        }

    }

    return current;

}

bool TxActor::isRelativeTip( t_ptrTx toCheck, std::vector<t_ptrTx>& tips )
{
    // if tips is sorted when txactor receives it, we can improve performance as this is called alot during tip selection
    std::vector<t_ptrTx>::iterator it = std::find( tips.begin(), tips.end(), toCheck );

    if( it == tips.end() )
    {
        return false;
    }
    else
    {
        return true;
    }

}

void TxActor::filterView( std::vector<t_ptrTx>& view, omnetpp::simtime_t timeStamp )
{

    std::vector<int> removeIndexes;

    for( int i = 0; i < view.size(); ++i )
    {
        if( view.at( i )->timeStamp > timeStamp )
        {
            removeIndexes.push_back( i );
        }
    }

    if( removeIndexes.size() > 0 )
    {
        for( int i = removeIndexes.size() -1; i > -1; --i )
        {
            view.erase( view.begin() + removeIndexes.at( i ) );
        }
    }

}

int TxActor::findMaxWeightIndex(std::vector<t_ptrTx>& view, omnetpp::simtime_t timeStamp )
{
    int maxWeight = 0;
    int maxWeightIndex = 0;

    for( int i = 0; i < view.size(); ++i )
    {
        int weight = ComputeWeight( view.at( i ), timeStamp );

        if( weight > maxWeight )
        {
            maxWeightIndex = i;
        }

    }

    return maxWeightIndex;
}

//TxActor def END

