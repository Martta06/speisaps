Te dejo **dos prompts independientes**, uno para cada persona. Incluyen el mismo reparto y formato de intercambio para que las dos IA propongan soluciones compatibles.

Actúa como desarrollador responsable del terreno, el cálculo de rutas y la búsqueda de campamentos en un hackathon de la NASA. Ayúdame a concretar e implementar mi parte de una web que estamos desarrollando entre tres personas.

Queremos construir un Google Maps marciano centrado inicialmente en una zona pequeña del cráter Gale. El usuario selecciona origen y destino, compara rutas y sigue un recorrido simulado. Las rutas tienen que considerar distancia, pendientes, obstáculos, condiciones meteorológicas y exposición estimada a radiación. Durante el trayecto puede pulsar «Necesito acampar» para encontrar un lugar adecuado y una ruta hasta él.

El reparto del equipo es el siguiente.

- Persona 1 desarrolla la web, el mapa, las capas visuales, los controles y la representación de las rutas y alertas.
- Yo soy la persona 2. Desarrollo el terreno, el grafo de navegación, el algoritmo de rutas y la búsqueda de campamentos.
- Persona 3 desarrolla la previsión meteorológica, la estimación de radiación y el detector simulado de peligros, incluidos objetos y tormentas de polvo. Me proporciona condiciones y peligros con posición y tiempo de validez.

Mis tareas son estas.

1. Elegir con el equipo una zona acotada de Gale y representar el terreno mediante una malla o un grafo. Investigar fuentes de elevación y sus resoluciones. Explicar qué se puede calcular con cada fuente y usar datos sintéticos identificados si falta información.

2. Calcular distancias y pendientes de los tramos. Si disponemos de datos adecuados, incorporar rugosidad y obstáculos. Las coordenadas geográficas marcianas deben convertirse de forma coherente a distancias métricas para los cálculos.

3. Implementar rutas mediante A* o Dijkstra. Separar las restricciones que bloquean un tramo de los factores que incrementan su coste. Normalizar los factores antes de combinarlos y proponer valores predeterminados para minimizar parámetros manuales.

4. Comparar una ruta corta con otra que reduzca los riesgos modelados. Para cada alternativa, devolver geometría, distancia, duración estimada, pendiente máxima, exposición acumulada estimada y motivos de elección. Definir un modo de desplazamiento y una velocidad coherentes para la demo.

5. Estimar la hora de llegada a cada tramo y consultar las condiciones que proporciona la persona 3 para ese momento. Ante una alerta, comprobar si afecta al recorrido pendiente y permitir recalcular desde la posición actual.

6. Implementar la búsqueda de campamentos. Buscar superficies verdes contiguas de tamaño suficiente, con pendiente adecuada y acceso viable. Evaluar también los peligros previstos durante la llegada y la estancia. Devolver candidatos ordenados, su explicación y la ruta hasta ellos. Representar como desconocidas las celdas con información insuficiente y devolver un resultado explícito si no existe un candidato válido.

7. Preparar la estructura para sustituir los datos de Gale por los de otra región.

Podemos reutilizar un proyecto previo llamado Isopath, desarrollado con Python y NetworkX para buscar rutas evitando pendientes superiores a un umbral. Antes de asumir que tienes acceso a su código, pídeme los archivos necesarios o prepara una implementación compatible.

Coordínate con los siguientes acuerdos de integración propuestos.

- El terreno y las rutas se intercambian como GeoJSON. Acordad explícitamente sistema de referencia marciano, orden de coordenadas, convención de longitudes y unidades.
- Para la demo, el tiempo se expresa en segundos desde el inicio del escenario, con un instante inicial documentado.
- La persona 3 entrega condiciones con variable, valor, unidad, ubicación o zona de aplicación, tiempo de validez y procedencia.
- Cada peligro incluye identificador, tipo, geometría, intervalo de actividad, severidad y procedencia. Las tormentas incluyen geometrías previstas en distintos momentos.
- Mi módulo consume esos datos y devuelve rutas y campamentos. La persona 1 representa los resultados y controla la interacción.
- Los datos observados, derivados y simulados deben conservar su procedencia en todas las respuestas.

Empieza proponiendo un MVP realizable durante un hackathon, el orden de implementación y los contratos de datos concretos. Incluye ejemplos pequeños de entradas y salidas que pueda pasar directamente a las otras dos personas. Después ayúdame a implementar y verificar el recorrido completo con una ruta normal, un obstáculo y una tormenta que obligue a desviarse o buscar campamento.

Verifica las fuentes y afirmaciones técnicas que utilices. Explica cualquier supuesto relevante y evita ampliar el alcance antes de que funcione la integración básica.

Actúa como desarrollador responsable de meteorología, radiación y detección de peligros en un hackathon de la NASA. Ayúdame a concretar e implementar mi parte de una web que estamos desarrollando entre tres personas.

Queremos construir un Google Maps marciano centrado inicialmente en una zona pequeña del cráter Gale. El usuario selecciona origen y destino, compara rutas y sigue un recorrido simulado. Las rutas consideran distancia, pendientes, obstáculos, meteorología y exposición estimada a radiación. Durante el trayecto puede pulsar «Necesito acampar» y recibir una recomendación de campamento accesible.

El reparto del equipo es el siguiente.

- Persona 1 desarrolla la web, el mapa, las capas visuales, los controles y la representación del radar y las alertas.
- Persona 2 desarrolla el terreno, el grafo, el cálculo y recálculo de rutas y la búsqueda de campamentos.
- Yo soy la persona 3. Proporciono las condiciones ambientales, las predicciones y los peligros que consumen los otros dos módulos.

Mis tareas son estas.

1. Investigar datos meteorológicos disponibles para Gale, por ejemplo los instrumentos de Curiosity y los archivos oficiales correspondientes. Verificar variables, cobertura temporal, unidades, resolución espacial y tratamiento de datos ausentes. Proponer una variable inicial que permita entrenar y evaluar un modelo durante el hackathon.

2. Construir una predicción meteorológica sencilla. Comparar un modelo básico de referencia, como la persistencia o el valor de la misma hora del sol anterior, con un modelo adicional si el tiempo lo permite. Reservar un tramo temporal para evaluar el error, evitando que información futura entre en el entrenamiento. Definir el horizonte de predicción y sus limitaciones.

3. Preparar la estimación de radiación. Investigar fuentes disponibles y distinguir mediciones, interpolaciones, predicciones y valores simulados. Entregar una tasa de dosis con unidad y tiempo de validez que permita a la persona 2 acumular exposición a lo largo de una ruta. Evitar atribuir variación espacial o protección del terreno sin un modelo que la justifique.

4. Desarrollar un detector tipo radar para el escenario de navegación. Incluir rocas u objetos que bloquean el camino, zonas con obstáculos y tormentas de polvo móviles. Implementar la lógica que determina qué peligros están dentro del alcance del detector, su distancia y dirección. La persona 1 se ocupa de dibujar el instrumento y los elementos en el mapa.

5. Modelar una tormenta de demostración con posición, extensión, velocidad y dirección. Generar su posición prevista en distintos momentos y la información necesaria para comprobar si coincidirá con el explorador. Documentar el carácter simulado de ese escenario y explicar qué datos y sensores harían falta para una detección física.

6. Entregar peligros con geometría y tiempo para que la persona 2 determine si intersectan la ruta a la hora prevista de paso. Acordar quién calcula cada alerta para evitar duplicar la lógica. La alerta resultante debe identificar peligro, tramo afectado, momento previsto y procedencia.

7. Facilitar la evaluación de campamentos proporcionando las condiciones previstas durante la llegada y la estancia. La persona 2 selecciona el campamento y calcula cómo alcanzarlo.

Coordínate con los siguientes acuerdos de integración propuestos.

- Utilizamos GeoJSON para geometrías. Acordad explícitamente sistema de referencia marciano, orden de coordenadas, convención de longitudes y unidades.
- Para la demo, el tiempo se expresa en segundos desde el inicio del escenario, con un instante inicial documentado.
- Cada condición incluye variable, valor, unidad, ubicación o zona de aplicación, tiempo de validez y procedencia.
- Cada peligro incluye identificador, tipo, geometría, intervalo de actividad, severidad y procedencia. Las tormentas incluyen geometrías previstas en distintos momentos.
- Si los datos meteorológicos representan toda la zona, entregar una previsión común y documentar esa resolución.
- Debe ser posible ejecutar la demostración con archivos locales y un reloj simulado, sin depender de una conexión en directo.

Prepara una demo reproducible en la que una roca aparezca cerca del explorador y una tormenta vaya a cruzar su ruta más adelante. El sistema debe permitir anticipar el encuentro, recalcular el recorrido y activar la búsqueda de campamento.

Empieza proponiendo un MVP realizable, el orden de implementación y los contratos de datos concretos. Incluye ejemplos pequeños de condiciones y peligros que pueda pasar a las personas 1 y 2 desde el principio. Después ayúdame a implementar y verificar el módulo.

Prioriza meteorología, radiación y detección de peligros. El registro de recursos para estancias largas, producción de glucosa y contaminación cruzada queda como ampliación cuando funcione el recorrido principal. Verifica las fuentes y afirmaciones científicas y explica los supuestos que adoptes.