/**
 * Classe pour les attributs d'un ESP
 */

class Esp {
    constructor(name, mac, map, lat, lon){
        this.name = name;
        this.mac = mac;
        this.map = map;
        this.lat = lat;
        this.lon = lon;
        this.temp = 0;
        this.data = undefined;
        this.marker = undefined;
        this.temperatureSerie = undefined;
        this.lightSerie = undefined;
        this.createMarker();
    }

    getTemperatureSerie(){
        return this.temperatureSerie;
    }

    setTemperatureSerie(temp){
        this.temperatureSerie = temp;
    }

    getLightSerie(){
        return this.lightSerie;
    }

    setLightSerie(light){
        this.lightSerie = light;
    }

    getMac(){
        return this.mac;
    }

    setData(data){
        this.data = data;
    }

    getName(){
        return this.name;
    }

    getData(){
        return this.data;
    }
    // Création du marker
    createMarker(){
        this.marker = L.marker([this.lat, this.lon]).addTo(this.map);
        this.marker.bindPopup(this.name);
    }
    // Mise à jour de la dernière temp et mise à jour du contenu de la popup du marker
    updateMarkerPopUpWithTemp(){
        this.getLastTemp();
        this.marker.setPopupContent(this.name+"<br>"+this.temp+" °C");
    }
    // récupération de la dernière temp
    getLastTemp(){
        this.temp = this.data[this.data.length-1][1];
    }

}