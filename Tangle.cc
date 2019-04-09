#include "Tangle.h"
#include <iostream>
#include <random>
#include <chrono>
#include <algorithm>
#include <utility>
#include <functional>

int TxActor::actorCount;
long int Tx::tx_totalCount;
int Tangle::TangleGiveTipsCount;

/*
    Tx DEFINITIONS
*/

//return true if Tx has approved at least one other transactions
bool Tx::hasApprovees()
{
    return m_TxApproved.size() > 0;
}

Tx::Tx() : m_walkBacktracks(0), TxNumber(tx_totalCount)
{
    tx_totalCount++;
}


//Tx def END

/*
    TANGLE DEFINITIONS
*/

Tangle::Tangle() try : m_genesisBlock( new Tx )
{
     m_tips[m_genesisBlock->TxNumber] =  m_genesisBlock;
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
std::map<int, t_ptrTx> Tangle::giveTips()
{
     return m_tips;
}

//checks the newly approved tips against the current tip vecotr held by tangle
//removes any that are still unconfirmed
void Tangle::ReconcileTips( const t_txApproved& removeTips )
{

    for(auto& tipSelected : removeTips)
    {

        auto it = m_tips.find( tipSelected->TxNumber );

        if( it != m_tips.end() )
        {
            m_tips.erase( it );
        }
    }

}

//adds a pointer to a newly added but as yet unconfirmed Tx to the tip list
void Tangle::addTip( t_ptrTx newTip )
{
     m_tips[newTip->TxNumber] = newTip;
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


// Tangle def END

/*
    TXACTOR DEFINITIONS
*/

//Constructor

TxActor::TxActor() { ++actorCount; }

//Tips to approve selected completely at random
t_txApproved TxActor::URTipSelection( std::map<int, t_ptrTx> tips )
{

     t_txApproved chosenTips;

     for ( int i = 0; i < APPROVE_VAL; ++i )
     {
         if(tips.size() > 0)
         {
             std::uniform_int_distribution<int> tipDist( 0, tips.size() -1 );
             int iterAdvances = tipDist( getTanglePtr()->getRandGen() );

             assert(iterAdvances < tips.size());

             auto beginIter = tips.begin();
             std::advance( beginIter, iterAdvances );

             chosenTips.push_back( beginIter->second );
             tips.erase( beginIter );

             if( tips.size() == 0 )
             {
                 break;
             }

         }
     }

     chosenTips.erase( std::unique( chosenTips.begin(), chosenTips.end() ), chosenTips.end() ) ;
     return chosenTips;
}

//creates a new transaction, selects tips for it to approve, then adds the new transaction to the tip list
//ready for approval by the proceeding transactions
void TxActor::attach( std::map<int, t_ptrTx>& storedTips, omnetpp::simtime_t attachTime, t_txApproved& chosen )
{
     try
     {

         //create new tx
         m_MyTx.emplace_back( new Tx() );
         m_MyTx.back()->m_issuedBy = this;
         m_MyTx.back()->timeStamp = attachTime;


         //add pointer to new Tx to tips selected, so they know who approved them
         for ( auto& tipSelected : chosen )
         {

             tipSelected->m_approvedBy.push_back( m_MyTx.back() );

             if( !( tipSelected->isApproved ) )
             {
                 //with firstApprovedTime and timeAttached as field - we can compute the age of a transaction
                 tipSelected->firstApprovedTime = attachTime;
                 tipSelected->isApproved = true;
             }

         }

         m_MyTx.back()->m_TxApproved = chosen;

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
int TxActor::_computeWeight( std::vector<t_ptrTx>& visited, t_ptrTx& current, omnetpp::simtime_t timeStamp )
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

t_ptrTx TxActor::getWalkStart( std::map<int, t_ptrTx>& tips, int backTrackDist )
{

    std::uniform_int_distribution<int> tipDist( 0, tips.size() -1 );
    int iterAdvances = tipDist( getTanglePtr()->getRandGen() );

    assert( iterAdvances < tips.size() );

    auto beginIter = tips.begin();

    if(tips.size() > 1)
    {
        std::advance( beginIter, iterAdvances );
    }

    t_ptrTx current = beginIter->second;

    int count = backTrackDist;
    int approvesIndex;

    //start backtrack
    //go until genesis block or reach backtrack distance
    while( !current->isGenesisBlock && count > 0 )
    {

        std::uniform_int_distribution<int> choice( 0, current->m_TxApproved.size() -1 );
        approvesIndex = choice( getTanglePtr()->getRandGen() ) ;

        assert( approvesIndex < current->m_TxApproved.size() );

        current = current->m_TxApproved.at( approvesIndex );


        --count;

    }

    return current;

}

t_ptrTx TxActor::WalkTipSelection( t_ptrTx start, double alphaVal, std::map<int, t_ptrTx>& tips, omnetpp::simtime_t timeStamp )
{

    // Used to determine the next Tx to walk to
    int walkCounts = 0;

    t_ptrTx current = start;

    //keep going until we reach a "tip" in relation to the view of the tangle that TxActor has
    while( !isRelativeTip( current, tips ) )
    {

        ++walkCounts;

        //copy of each transactions approvers
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

bool TxActor::isRelativeTip( t_ptrTx& toCheck, std::map<int, t_ptrTx>& tips )
{
    // if tips is sorted when txactor receives it, we can improve performance as this is called alot during tip selection
    auto it = tips.find( toCheck->TxNumber );

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

t_ptrTx TxActor::EasyWalkTipSelection( t_ptrTx start, double alphaVal, std::map<int, t_ptrTx>& tips, omnetpp::simtime_t timeStamp )
{

    // Used to determine the next Tx to walk to
    int walkCounts = 0;

    t_ptrTx current = start;

    //keep going until we reach a "tip" in relation to the view of the tangle that TxActor has
    while( !isRelativeTip( current, tips ) )
    {

        ++walkCounts;

        //copy of each transactions approvers
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

        // choose if calculate the weights of available transaction here
        std::uniform_real_distribution<double> walkChoice( 0.0, 1.0 );

            if( walkChoice( getTanglePtr()->getRandGen() ) < alphaVal)
            {
                //if more than one find the max weight
                //get heaviest tx to simplify choosing next
                int maxWeightIndex = findMaxWeightIndex( currentView, timeStamp );
                current =  currentView.at( maxWeightIndex );


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

    current->m_walkBacktracks = walkCounts;

    return current;

}

// Allows us to use walk tip selection with multiple walkers
t_txApproved TxActor::NKWalkTipSelection( double alphaVal, std::map<int, t_ptrTx>& tips, omnetpp::simtime_t timeStamp, int kMultiplier, int backTrackDist)
{
    // Walkers sent == 3 * k + 4
    int walkers = kMultiplier * APPROVE_VAL + 4;

    std::vector<t_ptrTx> vec_walkerResults;
    vec_walkerResults.reserve(walkers);

    // Let the walkers find the tips tips
    for( int i = 0; i < walkers; ++i )
    {
        vec_walkerResults.push_back( EasyWalkTipSelection( getWalkStart( tips, backTrackDist ) , alphaVal, tips, timeStamp ) );
    }

    // Sort tips by how many steps the walker made - ascending order
    std::sort( vec_walkerResults.begin(), vec_walkerResults.end(), [] ( t_ptrTx left, t_ptrTx right )
        {
            return left->m_walkBacktracks > right->m_walkBacktracks;
        }
    );

    // Dedupe so we dont approve the same tip more than once
    vec_walkerResults.erase( std::unique( vec_walkerResults.begin(), vec_walkerResults.end() ), vec_walkerResults.end() ) ;

    assert( vec_walkerResults.size() > 0 );

    int tipSelectedSize =  APPROVE_VAL > vec_walkerResults.size() ? vec_walkerResults.size() : APPROVE_VAL ;

    t_txApproved retVal(vec_walkerResults.begin(), vec_walkerResults.begin() + tipSelectedSize );


    return retVal;
}


