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
ClientMQTT.addSubscription("Eolienne/DataNode", async (message) => {
    console.log(message);
    var json = JSON.parse(message);   
    json.unixTimestamp =  Math.floor(Date.now() / 1000);
    //await MongoDB.pushData(json, "Eolienne", "data");
    await MongoDB.getCollection("Eolienne", "data").updateOne(
        { idMesh: json.idMesh },
        { $push: { "data": {"isBroken": json.isBroken, "speedPourcent": json.speedPourcent, "timestamp": json.unixTimestamp} } }
        , { upsert: true }
    )
});

ClientMQTT.addSubscription("Eolienne/Network", async (message) => {
    console.log(message);
    var json = JSON.parse(message);   
    json.unixTimestamp =  Math.floor(Date.now() / 1000);
    await MongoDB.pushData(json, "Eolienne", "network");
});

ClientMQTT.connect(uriMQTT);

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

app.get("/datas", async (req, res) => {
    var test = await MongoDB.getCollection("Eolienne", "data").aggregate(
    [
        { 
            $project: {
                "idMesh": "$idMesh",
                "data": { $arrayElemAt: [{$slice: [ "$data", -1]},0] },
            }
        }
    ]).toArray();
    res.json(test);
})

app.get("/datas/:id", async (req, res) => {

    var id = parseInt(req.params.id);

    var test = await MongoDB.getCollection("Eolienne", "data").aggregate([
        {$match: { 'idMesh': id }},
        {$unwind: '$data'},
        {$sort: { 'data.timestamp': -1 }},
        {$limit: 30},
        {$sort: { 'data.timestamp': 1 }}
      ]).toArray();
    res.json(test);
})

process.on('exit', (code) => {
    MongoDB.disconnectClient();
    ClientMQTT.disconnect();
})


export default app;


