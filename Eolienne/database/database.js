import pkg from 'mongodb';
const { MongoClient } = pkg;

var client = undefined;
const databaseMap = new Map();
const collectionMap = new Map();
const MongoDB = {};

MongoDB.clientIsAvailable = () => {
    return client != undefined && client.isConnected();
}

MongoDB.init = (uri) => {
    if(!MongoDB.clientIsAvailable()){
        client = new MongoClient(uri, {
            useNewUrlParser: true,
            useUnifiedTopology: true,
        });
    }
}

MongoDB.connectClient = async () => {
    if(!MongoDB.clientIsAvailable()){
        try {
            await client.connect();
        } catch(error){
            console.log(error);
            return false;
        }
    }
    return true;
}

MongoDB.disconnectClient = () => {
    if(MongoDB.clientIsAvailable())
        client.logout();
}

MongoDB.pushData = async (data, databaseName, collectionName) => {

    console.log("0")

    if(!MongoDB.clientIsAvailable())
        return;

    /*console.log("1")

    if(!databaseMap.has(databaseName)){
        databaseMap.set(databaseName,client.db(databaseName));
    }

    console.log("2")

    if(!collectionMap.has(collectionName)){
        collectionMap.set(collectionName,databaseMap.get(databaseName).collection(collectionName));
    }*/

    console.log("3")

    console.log(data)

    try{
        await client.db(databaseName).collection(collectionName).insertOne(data);
    } catch(e){
        console.log(e);
    }
    

    /*collectionMap.get(collectionName).insertOne(data, (err, res) => {
		if (err) {
            console.log(err);
            throw err;
        }
		console.log("\nItem : ", new_entry, "\ninserted in db in collection :", key);
	});*/
}

MongoDB.getCollection = (databaseName, collectionName) => {
    return client.db(databaseName).collection(collectionName);
}

export default MongoDB;