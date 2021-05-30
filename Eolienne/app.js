import Express from "express";
import cors  from "cors";
import MongoDB from "./database/database.js"
import ClientMQTT from "./mqtt/mqttClient.js"
import path from 'path';
const __dirname = path.resolve();

const uriMQTT = "mqtt://broker.hivemq.com";
const uriMongo = "mongodb://localhost:27017";

//Setup express
var whitelist = ["http://localhost:8080"]
var corsOptions = {
    origin: function (origin, callback) {
        console.log(origin);
        if (whitelist.indexOf(origin) !== -1) {
            callback(null, true)
        } else {
            callback(new Error('Not allowed by CORS'))
        }
    }
}

const app = Express();
app.use(cors());

//Setup database
MongoDB.init(uriMongo);
var resConnection = await MongoDB.connectClient();
if(!resConnection){
    //process.exit(1);
}

//Setup mqttClient
ClientMQTT.addSubscription("eolienne/data", async (message) => {
    var json = JSON.parse(message);   
    json.unixTimestamp =  Math.floor(Date.now() / 1000);
    await MongoDB.pushData(json, "Eolienne", "data");
});

ClientMQTT.addSubscription("eolienne/network", async (message) => {
    var json = JSON.parse(message);   
    json.unixTimestamp =  Math.floor(Date.now() / 1000);
    await MongoDB.pushData(json, "Eolienne", "network");
});

ClientMQTT.connect(uriMQTT);

process.on('exit', (code) => {
    MongoDB.disconnectClient();
    ClientMQTT.disconnect();
})

app.get('/', (req, res) => {
    res.sendFile('/website/index.html', {root: __dirname});
})

app.get("/stateConnection", (req, res) => {
    res.json({
        mqtt: ClientMQTT.isConnected(),
        database: MongoDB.clientIsAvailable()
    });
});

app.get("/network", (req, res) => {

    MongoDB.getCollection("Eolienne", "network").findOne({}, {sort: {unixTimestamp: -1 } }, function(err, result) {
        
        if (err){
            console.log(err)
            res.json({});
        }
        else {
            console.log(result)
            res.json(result);
        }
           
        
    });
    
})

export default app;


