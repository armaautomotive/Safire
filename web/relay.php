<?php
/**
* relay.php
*
* Description: This is a temporary server endpoint that will connect nodes to each other for the first time.
*       After a set of nodes is shared among nodes they can relay connection data for additional nodes.
*
* 
* @param 'sender_key' user public key to identify 
* @param 'receiver_key' recipient ID.
* @param 'action' sendmessage +(sender_key, receiver_key, message), getmessages +(receiver_key), getnodes +(sender_key) 
* @param 'message' json payload to be delivered.
*/
header('Content-Type: application/json');

//error_reporting(E_ALL);
//ini_set('display_errors', 1);

include_once("db.php");

date_default_timezone_set('America/Los_Angeles');

$sender_key = isset( $_POST['sender_key'] ) ? $_POST['sender_key'] : $_GET['sender_key'];
$receiver_key = isset( $_POST['receiver_key'] ) ? $_POST['receiver_key'] : $_GET['receiver_key'];
$connection_string = isset( $_POST['connection_string'] ) ? $_POST['connection_string'] : $_GET['connection_string'];
$action = isset( $_POST['action'] ) ? $_POST['action'] : $_GET['action']; 
$message = isset( $_POST['message'] ) ? $_POST['message'] : $_GET['message']; 
$type = isset( $_POST['type'] ) ? $_POST['type'] : $_GET['type']; 
$return_request_data = isset( $_POST['return_request_data'] ) ? $_POST['return_request_data'] : $_GET['return_request_data']; 

if($action == 'sendmessage' && $sender_key != '' && $message != ''){
	$pdo = new PDO('mysql:dbname='.$db_name.';host='.$db_host, $db_user, $db_pass);
        $params = array();
        $params["sender_key"] = $sender_key;
	$params["receiver_key"] = $receiver_key;
	$params["message"] = $message;
        $params["type"] = $type;
        $sql = "select * from messages where sender_key = :sender_key && receiver_key = :receiver_key && message = :message && type = :type && received = 0 order by time desc limit 0, 1;";
        $stmt = $pdo->prepare($sql);
        $r = $stmt->execute($params);
        $results = $stmt->fetchAll(PDO::FETCH_ASSOC);
        if( sizeof($results) == 0 ){
                // Add current user to the database.
                $params = array();
                $params["type"] = $type;
                $params["sender_key"] = $sender_key;
		$params["receiver_key"] = $receiver_key;
		$params["request"] = $request; // what is this?
		$params["message"] = $message;
                $insert_sql = "insert into messages (sender_key, receiver_key, request, message, type, received) values (:sender_key, :receiver_key, :request, :message, :type, 0);";
                $stmt = $pdo->prepare($insert_sql);
                $stmt->execute($params);
                
                // Purge old data
                $delete_sql = "delete from messages where time < UNIX_TIMESTAMP(DATE_SUB(NOW(), INTERVAL 3 DAY))";
                $stmt = $pdo->prepare($delete_sql);
                $result = $stmt->execute($params);
        	
		$response = array();
        	$response["response"] = "Message sent.";
        	echo json_encode($response);
        	exit;		
	} else {
		$response = array();
        	$response["response"] = "Duplicate message.";
        	echo json_encode($response);
        	exit;
	}
}

if($action == 'getmessages' && $receiver_key != ''){
	$pdo = new PDO('mysql:dbname='.$db_name.';host='.$db_host, $db_user, $db_pass);
	$sql = "select * from messages where receiver_key = :receiver_key && type = :type && received = 0 order by time desc limit 0, 100;";
        $stmt = $pdo->prepare($sql);
        $params = array();
        $params["receiver_key"] = $receiver_key;
        $params["type"] = $type;
        $r = $stmt->execute($params);
        $results = $stmt->fetchAll(PDO::FETCH_ASSOC);
        if( sizeof($results) > 0 ){
                $sent_ids = array();
		$data = array();
                $message_result = "";
                foreach($results as $result){
                        $data_record = array();
                        $data_record["receiver_key"] = $result["receiver_key"];
                        $data_record["sender_key"] = $result["sender_key"];
			$data_record["request"] = $result["request"];
			$data_record["message"] = $result["message"];	
                       	$data_record["id"] = $result["id"]; 
			array_push($data, $data_record);
			array_push($sent_ids, $result["id"]);

                        if($return_request_data == "true"){
                            $message_result .= json_encode($data_record);
                        } else {
                            $message_result .= $result["message"] . "\n";
                        }

			//$update_sql = "update from messages ".
                	//"set received = 1 ".
                	//"where id in (";
        	        //$stmt = $pdo->prepare($update_sql);
	                //$result = $stmt->execute($params);
 
                }
                //echo json_encode($data);
		echo $message_result;

		// Mark messages as received. 
		$update_sql = "update messages ".
		"set received = 1 ".
		"where id in (";
		$first = true;
                foreach($sent_ids as $id){
                        if(!$first){
                                $update_sql .= ", ";
                        }
                        $first = false;
                        $update_sql .= $id;
                }
		$update_sql .= ");";	 
		$stmt = $pdo->prepare($update_sql);
		$result = $stmt->execute($params);	
		//echo "Update: " . $result . " " . $update_sql . " ";

		// Delete messages
		//$delete_sql = "delete from messages where id in (";
		//$first = true;
		//foreach($sent_ids as $id){
		//	if(!$first){
		//		$delete_sql .= ", ";
		//	}
		//	$first = false;
		//	$delete_sql .= $id;
		//}	
		//$delete_sql .= ");";
		//$stmt = $pdo->prepare($delete_sql);
                //$result = $stmt->execute($params);
                exit;
        } else {
                // No messages are available
                $response = array();
                $response["response"] = "No messages.";
                $response["receiver_key"] = $receiver_key;
		$response["type"] = $type;
		echo json_encode($response);
                exit;
        }
}

if($action == 'getnodes' && $sender_key != ''){
	$pdo = new PDO('mysql:dbname='.$db_name.';host='.$db_host, $db_user, $db_pass);
	$params = array();
	$params["sender_key"] = $sender_key;
	$sql = "select * from node_list where public_key = :sender_key order by time desc limit 0, 1;";
	$stmt = $pdo->prepare($sql);
	$r = $stmt->execute($params);
	$results = $stmt->fetchAll(PDO::FETCH_ASSOC);
	if( sizeof($results) == 0 ){
	        // Add current user to the database.
	        $params = array();
	        $params["sender_key"] = $sender_key;
	        $params["connection_string"] = ""; // $connection_string;
	        $insert_sql = "insert into node_list (public_key, connection_string) values (:sender_key, :connection_string);";
	        $stmt = $pdo->prepare($insert_sql);
	        $stmt->execute($params);
	
	        // Purge old data
	        $delete_sql = "delete from node_list where time < UNIX_TIMESTAMP(DATE_SUB(NOW(), INTERVAL 2 DAY))";
	        $stmt = $pdo->prepare($delete_sql);
	        $result = $stmt->execute($params);
	}

	$sql = "select * from node_list where public_key != :sender_key order by time desc limit 0, 100;";
	$stmt = $pdo->prepare($sql);
	$params = array();
	$params["sender_key"] = $sender_key;
	$r = $stmt->execute($params);
	$results = $stmt->fetchAll(PDO::FETCH_ASSOC);
	if( sizeof($results) > 0 ){
	        $data = array();
	        foreach($results as $result){
	                $data_record = array();
	                $data_record["public_key"] = $result["public_key"];
	                $data_record["connection_string"] = $result["connection_string"];
	                array_push($data, $data_record);
	        }
	        echo json_encode($data);
	        exit;
	} else {
	        // No node is available
	        $response = array();
	        $response["error"] = "Error: There are currently no nodes available to connect to.";
	        echo json_encode($response);
	        exit;
	}
}

if(true){
        $response = array();
        $response["response"] = "Request not recognized.";
        echo json_encode($response);
        exit;
}

?>

