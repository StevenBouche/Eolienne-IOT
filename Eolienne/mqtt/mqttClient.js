import mqtt from 'mqtt';

const ClientMQTT = {
    
};
var client = undefined;
const mapAction = new Map();

const onMessage = (topic, message) => {

    console.log("Message received from topics : ", topic);
    console.log("Message : ", message.toString());
    if(mapAction.has(topic))
        mapAction.get(topic)(message);
}

const onConnect = () => {
    console.log("MQTT client connected.")
    client.subscribe([...mapAction.keys()]);
}

ClientMQTT.isConnected = () => {
    return client != undefined ? client.connected : false;
}

ClientMQTT.addSubscription = (topicName, callback) => {
    mapAction.set(topicName, callback);
}

ClientMQTT.connect = (uri) => {

    console.log("MQTT client trying connection.");

    client = mqtt.connect(uri);
    client.on('connect', onConnect);
    client.on('message', onMessage);
}

ClientMQTT.disconnect = () => {
    if(isConnected())
        client.end(true);
}

export default ClientMQTT;

