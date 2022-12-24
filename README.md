# HumansVsZombies

## History:
This plug-in is originally written by mrapple. Later I updated it to 2.4.x and currently it's being maintained here.

## Server Options:
This is a known working option for the HumansVsZombies game mode:
`bzfs -g +f SR{100} -loadplugin /path/to/plugin/zombie.so -set _reloadTime 3.0 -mp 100 -set _tankSpeed 35`


The following is a recommended options/settings to be used:
```
  -tk
  -ms 1
  -a 0 0
  -autoTeam
  -mp 100
  -set _shotSpeed 300
  -set _reloadTime 3.0
  -set _maxFlagGrabs 1
  -set _srRadiusMult 5
  -set _tankSpeed 35
  -set _flagPoleSize 15
  -set _flagPoleWidth .09
  -set _shotSpeed 200
  -set _tankSpeed 50
  -set _spawnSafeSRMod 10
  -set _tankAngVel 0.9
```

Do note that if no custom flag handling is done via another plug-in, it is recommended to add the following to the world file to be hosted:

```
position 10000 10000 0
size 10 10 10
zoneflag SR 100
end
```
