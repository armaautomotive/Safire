<?php
/**
* getnode.php
*
* Description: This is a temporary server endpoint that will connect nodes to each other for the first time.
*       After a set of nodes is shared among nodes they can relay connection data for additional nodes.
*
* @param 'r' p2p connection string
*/
header('Content-Type: application/json');

include_once("db.php");

date_default_timezone_set('America/Los_Angeles');

$clear = $_GET['clear'];
if($clear == "true"){
        $pdo = new PDO('mysql:dbname='.$db_name.';host='.$db_host, $db_user, $db_pass);
        $params = array();
        $sql = "delete from node_list;";
        $stmt = $pdo->prepare($sql);
        $stmt->execute($params);

        $response = array();
        $response["result"] = "Records cleared.";
        echo json_encode($response);
        exit;
}

$requester = $_GET['r'];
if($requester == ''){
        $response = array();
        $response["error"] = "Error: no 'r' parameter provided.";
        echo json_encode($response);
        exit;
}

$pdo = new PDO('mysql:dbname='.$db_name.';host='.$db_host, $db_user, $db_pass);
$params = array();
$params["connection_string"] = $_GET['r'];
$sql = "select * from node_list where connection_string = :connection_string order by time desc limit 0, 1;";
$stmt = $pdo->prepare($sql);
$r = $stmt->execute($params);
$results = $stmt->fetchAll(PDO::FETCH_ASSOC);
if( sizeof($results) == 0 ){
        // Add current user to the database.
        $insert_sql = "insert into node_list (connection_string) values (:connection_string);";
        $stmt = $pdo->prepare($insert_sql);
        $stmt->execute($params);

	// Purge old data
        $delete_sql = "delete from node_list where time < UNIX_TIMESTAMP(DATE_SUB(NOW(), INTERVAL 30 DAY))";
        $stmt = $pdo->prepare($delete_sql);
        $result = $stmt->execute($params);
}

$sql = "select * from node_list where connection_string != :connection_string order by time desc limit 0, 1;";
$stmt = $pdo->prepare($sql);
$r = $stmt->execute($params);
$results = $stmt->fetchAll(PDO::FETCH_ASSOC);
if( sizeof($results) > 0 ){
        $result = $results[0];
        $response = array();
        $response["connection_string"] = $result["connection_string"];
        echo json_encode($response);
        exit;
} else {
        // No node is available
        $response = array();
        $response["error"] = "Error: There are currently no nodes available to connect to.";
        echo json_encode($response);
        exit;
}
?>

