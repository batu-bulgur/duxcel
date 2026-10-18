import {registerHooks} from 'node:module';
import {pathToFileURL} from 'node:url';
registerHooks({resolve(specifier,context,next){
    return next(specifier,specifier==='apache-arrow'?{...context,parentURL:pathToFileURL('/Users/h2o/dev/duxcel/web/package.json').href}:context);
}});
