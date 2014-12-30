function calc_score(params, context, done) {
    var admin = context.getAppAdminContext();
    
    var object = admin.objectWithURI(params.uri);
    object.refresh({
            success: function(theObject) {
                var app_id = theObject.get('app_id');
                retrieve_comments(admin, app_id, done);
            },
            failure: function(theObject, errorMsg) {
                done(errorMsg);
            }
        }
    );

}

function retrieve_comments(admin, app_id, done) {
    var bucket = admin.bucketWithName('comments');
    var clause = KiiClause.equals("app_id", app_id);
    var query = KiiQuery.queryWithClause(clause);
    var all_scores = [];

    var queryCallbacks = {
        success: function(queryPerformed, resultSet, nextQuery) {
            for(var i=0; i<resultSet.length; i++) {
                all_scores[all_scores.length] = resultSet[i].get('score');
            }
            if (nextQuery != null) {
                bucket.executeQuery(nextQuery, queryCallbacks);
            } else {
                update_app(admin, app_id, avg(all_scores), done);
            }
        },
        failure: function(queryPerformed, anErrorString){
            done(anErrorString);
        }
    };
    bucket.executeQuery(query, queryCallbacks);
}

function avg(scores) {
    if (scores.length == 0) {
        return 0;
    }
    var sum = 0;
    for (var i=0; i<scores.length; i++) {
        sum += scores[i];
    }
    return sum/scores.length;
}

function update_app(admin, app_id, score, done) {
    var bucket = admin.bucketWithName('apps');
    var clause = KiiClause.equals("app_id", app_id);
    var query = KiiQuery.queryWithClause(clause);
    
    var queryCallbacks = {
        success: function(queryPerformed, resultSet, nextQuery) {
            if (resultSet.length != 1) {
                done("None or more than one app are found: " + resultSet.length);
                return;
            }
            app = resultSet[0];
            app.set('rating_score', score);
            save_app(app, done);
        },  
        failure: function(queryPerformed, anErrorString){
            done(anErrorString);
        }   
    };  
    bucket.executeQuery(query, queryCallbacks);
}

function save_app(appObject, done) {
    appObject.save({
        success: function(theObject) {
        done("Object saved!" + theObject.getUUID());
    },
    failure: function(theObject, errorString) {
        done("Error saving object: " + errorString);
    }
    });    

}
