<?php
/**
* getnode.php
*
* Description: This is a temporary server endpoint that will connect nodes to each other for the first time.
*       After a set of nodes is shared among nodes they can relay connection data for additional nodes.
*
* TODO: This endpoint needs to keep track of connection pairs to that both nodes receive the others connection string.
* 
* @param 'connection_string' p2p connection string
* @param 'public_key' user public key to identify 
*/
header('Content-Type: application/json');

include_once("db.php");

date_default_timezone_set('America/Los_Angeles');

$public_key = isset( $_POST['public_key'] ) ? $_POST['public_key'] : $_GET['public_key'];
$connection_string = isset( $_POST['connection_string'] ) ? $_POST['connection_string'] : $_GET['connection_string'];

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

if($connection_string == ''){
        $response = array();
        $response["error"] = "Error: no 'connection_string' parameter provided.";
        echo json_encode($response);
        exit;
}
if($public_key == ''){
        $response = array();
        $response["error"] = "Error: no 'public_key' parameter provided.";
        echo json_encode($response);
        exit;
}
$pdo = new PDO('mysql:dbname='.$db_name.';host='.$db_host, $db_user, $db_pass);
$params = array();
$params["public_key"] = $public_key;
$sql = "select * from node_list where public_key = :public_key order by time desc limit 0, 1;";
$stmt = $pdo->prepare($sql);
$r = $stmt->execute($params);
$results = $stmt->fetchAll(PDO::FETCH_ASSOC);
if( sizeof($results) == 0 ){
        // Add current user to the database.
        $params = array();
        $params["public_key"] = $public_key;
        $params["connection_string"] = $connection_string;
        $insert_sql = "insert into node_list (public_key, connection_string) values (:public_key, :connection_string);";
        $stmt = $pdo->prepare($insert_sql);
        $stmt->execute($params);

        // Purge old data
        $delete_sql = "delete from node_list where time < UNIX_TIMESTAMP(DATE_SUB(NOW(), INTERVAL 30 DAY))";
        $stmt = $pdo->prepare($delete_sql);
        $result = $stmt->execute($params);
}

$sql = "select * from node_list where public_key != :public_key order by time desc limit 0, 100;";
$stmt = $pdo->prepare($sql);
$params = array();
$params["public_key"] = $public_key;
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
?>



