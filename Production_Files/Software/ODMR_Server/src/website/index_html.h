#ifndef INDEX_HTML_H
#define INDEX_HTML_H

const char INDEX_HTML[] PROGMEM = R"====(
<!DOCTYPE HTML>
<html lang="de">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>NV Experimente / ODMR</title>
    <link rel="icon" href="data:,">
    <link rel="stylesheet" type="text/css" href="style.css">
</head>
<body>
    <header>
        <nav>
            <ul>
                <li><a href="index.html" class="open">Start</a></li>
                <li><a href="messung.html">Messung</a></li>
                <li><a href="justage.html">Justage (folgt)</a></li>
                <li><a href="infos.html">Weitere Informationen</a></li>
            </ul>
        </nav>
        <h1>Low-Cost Experimente mit NV-Zentren</h1>
    </header>
    <main>
        <article>
            <h2>Über das Experiment</h2>
            <p>
                NV-Zentren sind Fehlstellen, die in Diamanten auftreten können. An diesen Stellen fehlen zwei Kohlenstoffatome im Gitter. 
                Anstatt eines dieser Kohlstoffatome ist ein Stickstoffatom im Gitter vorhanden. Anstatt des zweiten fehlenden Kohlen&shy;stoff&shy;atoms
                ist kein weiteres Atom sondern eine Fehlstelle im Gitter vorhanden. Dies führt dazu, dass sich an der Stelle im Gitter Elektronen befinden,
                die angeregt werden können und dadurch einige wichtige. Die Abbildung 1 zeigt diese Diamantstruktur. 
            </p>
            <p align="center">
                [Abbildung 1: Struktur des Diamantgitters - Bild nicht verfügbar in Header-Version]
            </p>
            <p> 
                Die Elektronen solcher Fehlstellen können mit grünem Licht in einen angeregten Zustand gebracht werden. Beim Übergang vom angeregten Zustand 
                zurück in den Grundzustand wird ein rotes oder nahinfrarotes Photon emitiert. 
            </p>
        </article>
        <article>
            <h3>Anwendungen</h3>
        <p> 
            Diese Emmission kann zur Messung von Magnetfeldern genutzt werden. Außerdem lässt sich durch das 2- bzw. 3-Zustandssystem, dass sich aus den Spinzuständen ergibt, ein einfach Quantencomputer bauen. 
        </p>
        </article>
    </main>
    <footer>
        <p> Uni Münster - info@o3q.de</p>
    </footer>
   
</body>
</html>
)====";

#endif // INDEX_HTML_H