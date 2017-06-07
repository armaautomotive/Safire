




/**
* validateTime
*
* Description: Given a list of records (queue or block)
*  validate that a block was generated at the assigned time for a given user. 
* 1) Time Attack Validation 
*  When a user joins the network a consensus time is embeded into the join record that 
*  becomes part of the block chain. All subsiquent blocks created by that user must match that 
*  join time plus the amount of time elapsed for subsiquent blocks. 
*   i.e. time attack - a user can't change their time after joining to alter who can create a block. 
*
*/
int CValidation::validateTime( records ){

	return 0;
}


int CValidation::sufficientFunds( records ){

	return 0;
}


int CValidation::validateSignature( records ){

	return 0
}

/**
* longestChain
*
* Description: confirm block chain data is the longest in set.
*/
int CValidation::longestChain() {

	return 0;
}

/**
* validateChain
*
* Descripttion: validate current block chain,
*/
int CValidation::validateChain(){
	
	return 0;
}


