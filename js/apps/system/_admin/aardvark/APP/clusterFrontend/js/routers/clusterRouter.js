/*global window, $, Backbone, document, arangoCollectionModel,arangoHelper,
arangoDatabase, btoa, _*/

(function() {
  "use strict";

  window.ClusterRouter = Backbone.Router.extend({

    routes: {
      ""                       : "initialRoute",
      "planScenario"           : "planScenario",
      "planTest"               : "planTest",
      "planAsymmetrical"       : "planAsymmetric",
      "shards"                 : "showShards",
      "showCluster"            : "showCluster",
      "handleClusterDown"      : "handleClusterDown"
    },

    // Quick fix for server authentication
    addAuth: function (xhr) {
      var u = this.clusterPlan.get("user");
      if (!u) {
        xhr.abort();
        if (!this.isCheckingUser) {
          this.requestAuth();
        }
        return;
      }
      var user = u.name;
      var pass = u.passwd;
      var token = user.concat(":", pass);
      xhr.setRequestHeader('Authorization', "Basic " + btoa(token));
    },

    requestAuth: function() {
      this.isCheckingUser = true;
      this.clusterPlan.set({"user": null});
      var modalLogin = new window.LoginModalView();
      modalLogin.render();
    },

    getNewRoute: function(last) {
      if (last === "statistics") {
        return this.clusterPlan.getCoordinator()
          + "/_admin/"
          + last;
      }
      return this.clusterPlan.getCoordinator()
        + "/_admin/aardvark/cluster/"
        + last;
    },

    initialRoute: function() {
      this.initial();
    },

    updateAllUrls: function() {
      _.each(this.toUpdate, function(u) {
        u.updateUrl();
      });
    },

    registerForUpdate: function(o) {
      this.toUpdate.push(o);
      o.updateUrl();
    },

    initialize: function () {
      this.footerView = new window.FooterView();
      this.footerView.render();

      var self = this;
      this.dygraphConfig = window.dygraphConfig;
      window.modalView = new window.ModalView();
      this.initial = this.planScenario;
      this.isCheckingUser = false;
      this.bind('all', function(trigger, args) {
        var routeData = trigger.split(":");
        if (trigger === "route") {
          if (args !== "showCluster") {
            if (self.showClusterView) {
              self.showClusterView.stopUpdating();
              self.shutdownView.unrender();
            }
            if (self.dashboardView) {
              self.dashboardView.stopUpdating();
            }
          }
        }
      });
      this.toUpdate = [];
      this.clusterPlan = new window.ClusterPlan();
      this.clusterPlan.fetch({
        async: false
      });
      $(window).resize(function() {
        self.handleResize();
      });
    },

    showCluster: function() {
      if (!this.showClusterView) {
        this.showClusterView = new window.ShowClusterView(
            {dygraphConfig : this.dygraphConfig}
      );
      }
      if (!this.shutdownView) {
        this.shutdownView = new window.ShutdownButtonView({
          overview: this.showClusterView
        });
      }
      this.shutdownView.render();
      this.showClusterView.render();
    },

    showShards: function() {
      if (!this.showShardsView) {
        this.showShardsView = new window.ShowShardsView();
      }
      this.showShardsView.render();
    },

    handleResize: function() {
        if (this.dashboardView) {
            this.dashboardView.resize();
        }
        if (this.showClusterView) {
            this.showClusterView.resize();
        }
    },

    planTest: function() {
      if (!this.planTestView) {
        this.planTestView = new window.PlanTestView(
          {model : this.clusterPlan}
        );
      }
      this.planTestView.render();
    },

    planAsymmetric: function() {
      if (!this.planSymmetricView) {
        this.planSymmetricView = new window.PlanSymmetricView(
          {model : this.clusterPlan}
        );
      }
      this.planSymmetricView.render(false);
    },

    planScenario: function() {
      if (!this.planScenarioSelector) {
        this.planScenarioSelector = new window.PlanScenarioSelectorView();
      }
      this.planScenarioSelector.render();
    },

    handleClusterDown : function() {
      if (!this.clusterDownView) {
        this.clusterDownView = new window.ClusterDownView();
      }
      this.clusterDownView.render();
    },

    dashboard: function() {
      var server = this.serverToShow;
      if (!server) {
        this.navigate("", {trigger: true});
        return;
      }

      server.addAuth = this.addAuth.bind(this);
      this.dashboardView = new window.ServerDashboardView({
        dygraphConfig: this.dygraphConfig,
        serverToShow : this.serverToShow,
        database: {
          hasSystemAccess: function() {
            return true;
          }
        }
      });
      this.dashboardView.render();
    },

    clusterUnreachable: function() {
      if (this.showClusterView) {
        this.showClusterView.stopUpdating();
        this.shutdownView.unrender();
      }
      if (!this.unreachableView) {
        this.unreachableView = new window.ClusterUnreachableView();
      }
      this.unreachableView.render();
    }

  });

}());
